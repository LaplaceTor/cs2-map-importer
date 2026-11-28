#include "Domain/Material/MaterialProxyCompiler.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

#include "Domain/Material/VmtParser.h"

namespace Domain::Material {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kEps = 1e-6;

// ============================================================================
// Formatting & Deterministic Hash Helpers
// ============================================================================

QString formatStaticFloat(double value)
{
    if (!std::isfinite(value)) {
        value = 0.0;
    }
    if (std::abs(value) < 1e-7) {
        value = 0.0;
    }
    return QString::number(value, 'f', 6);
}

QString formatStaticVec2(double x, double y)
{
    return QStringLiteral("[%1 %2]").arg(formatStaticFloat(x), formatStaticFloat(y));
}

QString formatStaticVec4(double x, double y, double z, double w)
{
    return QStringLiteral("[%1 %2 %3 %4]")
        .arg(formatStaticFloat(x), formatStaticFloat(y), formatStaticFloat(z), formatStaticFloat(w));
}

QString formatExprFloat(double value)
{
    if (!std::isfinite(value)) {
        value = 0.0;
    }
    if (std::abs(value) < 1e-7) {
        return QStringLiteral("0.0");
    }
    QString s = QString::number(value, 'f', 6);
    if (s.contains(u'.')) {
        while (s.endsWith(u'0') && !s.endsWith(QStringLiteral(".0"))) {
            s.chop(1);
        }
    } else {
        s.append(QStringLiteral(".0"));
    }
    if (s == QStringLiteral("-0.0")) {
        return QStringLiteral("0.0");
    }
    return s;
}

double deterministicSeed01(const QString& relativePath) noexcept
{
    const QByteArray bytes = relativePath.toLower().toUtf8();
    std::uint32_t hash = 2166136261u;
    for (const char ch : bytes) {
        hash ^= static_cast<std::uint8_t>(ch);
        hash *= 16777619u;
    }
    // Map to [0.0, 1.0)
    return static_cast<double>(hash) / 4294967296.0;
}

void upsertPair(QVector<QPair<QString, QString>>& list, const QString& key, const QString& value)
{
    for (auto& entry : list) {
        if (entry.first == key) {
            entry.second = value;
            return;
        }
    }
    list.push_back({key, value});
}

// ============================================================================
// Variable Reference & Literal Parsing
// ============================================================================

struct VarRef {
    QString baseName; // lowercase, e.g. "$color"
    int component = -1; // -1 = whole var, 0..3 = indexed component
};

std::optional<VarRef> parseVarRef(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (!trimmed.startsWith(u'$')) {
        return std::nullopt;
    }
    const QString lower = trimmed.toLower();
    const qsizetype bracketOpen = lower.indexOf(u'[');
    if (bracketOpen < 0) {
        return VarRef{lower, -1};
    }
    const qsizetype bracketClose = lower.indexOf(u']', bracketOpen + 1);
    if (bracketClose < 0) {
        return VarRef{lower.left(bracketOpen).trimmed(), -1};
    }
    bool ok = false;
    const int idx = lower.mid(bracketOpen + 1, bracketClose - bracketOpen - 1).trimmed().toInt(&ok);
    if (!ok || idx < 0 || idx > 3) {
        return std::nullopt;
    }
    return VarRef{lower.left(bracketOpen).trimmed(), idx};
}

struct ParsedLiteral {
    int width = 1;
    std::array<double, 4> values = {0.0, 0.0, 0.0, 0.0};
};

std::optional<ParsedLiteral> parseLiteral(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(u'$')) {
        return std::nullopt;
    }

    // Byte color {R G B} -> sRGB [0..1]
    if (trimmed.startsWith(u'{') && trimmed.endsWith(u'}')) {
        const auto color = VmtParser::parseColor(trimmed);
        if (!color.has_value()) {
            return std::nullopt;
        }
        return ParsedLiteral{3, {color->r, color->g, color->b, 1.0}};
    }

    // Bracket vector [a b ...]
    if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
        const QString inner = trimmed.mid(1, trimmed.size() - 2).trimmed();
        const QStringList parts = inner.split(u' ', Qt::SkipEmptyParts);
        if (parts.isEmpty() || parts.size() > 4) {
            return std::nullopt;
        }
        ParsedLiteral lit;
        lit.width = static_cast<int>(parts.size());
        for (int i = 0; i < lit.width; ++i) {
            bool ok = false;
            const double v = parts[i].toDouble(&ok);
            if (!ok || !std::isfinite(v)) {
                return std::nullopt;
            }
            lit.values[i] = v;
        }
        return lit;
    }

    // Plain scalar number
    bool ok = false;
    const double v = trimmed.toDouble(&ok);
    if (ok && std::isfinite(v)) {
        return ParsedLiteral{1, {v, v, v, v}};
    }
    return std::nullopt;
}

bool isColorLikeVar(const QString& varName)
{
    return varName == QLatin1String("$color")
        || varName == QLatin1String("$color2")
        || varName == QLatin1String("$selfillumtint")
        || varName == QLatin1String("$envmaptint")
        || varName == QLatin1String("$refracttint")
        || varName == QLatin1String("$reflecttint")
        || varName == QLatin1String("$fogcolor");
}

bool isTransformVar(const QString& varName)
{
    return varName == QLatin1String("$basetexturetransform")
        || varName == QLatin1String("$basetexturetransform2")
        || varName == QLatin1String("$texture2transform")
        || varName == QLatin1String("$detailtexturetransform")
        || varName == QLatin1String("$selfillumtexturetransform")
        || varName == QLatin1String("$bumptransform");
}

// ============================================================================
// SSA Expression DAG Arena (with Constant Folding & Value Numbering)
// ============================================================================

enum class ExprOp : int {
    Const,
    Time,
    UniformRandom,   // random(a, b) - impure, never deduplicated
    RenderAttr,      // exists(name) ? (name * a) : b  (or exists(name) ? name : b when a < 0)
    Add,
    Sub,
    Mul,
    Div,
    Mod,             // positive modulo ((a % b) + b) % b
    RawMod,          // (a % b) when numerator is guaranteed non-negative
    Neg,
    Sin,
    Cos,
    Abs,
    Frac,
    Floor,
    Min,
    Max,
    Clamp,
    Lerp,
    RemapValClamped,
    LessOrEqual,     // ((a <= b) ? c : d)
    Equal,           // ((a == b) ? c : d)
};

struct ExprNode {
    ExprOp op = ExprOp::Const;
    double constVal = 0.0;
    QString attrName;
    std::array<int, 5> args = {-1, -1, -1, -1, -1};
    int argCount = 0;
    bool isDynamic = false;
    bool isPure = true;
    bool containsLinearRamp = false;
};

struct ExprKey {
    ExprOp op = ExprOp::Const;
    std::uint64_t constBits = 0;
    QString attrName;
    std::array<int, 5> args = {-1, -1, -1, -1, -1};
    int argCount = 0;
    bool containsLinearRamp = false;

    bool operator==(const ExprKey& other) const noexcept = default;
};

size_t qHash(const ExprKey& k, size_t seed = 0) noexcept
{
    size_t h = ::qHash(static_cast<int>(k.op), seed);
    h ^= ::qHash(k.constBits, seed + 0x9e3779b9u) + 0x9e3779b9u + (h << 6) + (h >> 2);
    if (!k.attrName.isEmpty()) {
        h ^= ::qHash(k.attrName, seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    }
    for (int i = 0; i < k.argCount; ++i) {
        h ^= ::qHash(k.args[i], seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    }
    h ^= ::qHash(k.containsLinearRamp ? 1 : 0, seed) + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

class ExprArena {
public:
    const ExprNode& node(int id) const { return m_nodes[static_cast<size_t>(id)]; }

    bool isConst(int id) const { return id >= 0 && node(id).op == ExprOp::Const; }

    double constVal(int id) const { return node(id).constVal; }

    bool isZero(int id) const { return isConst(id) && std::abs(constVal(id)) <= kEps; }

    bool isOne(int id) const { return isConst(id) && std::abs(constVal(id) - 1.0) <= kEps; }

    int makeConst(double v)
    {
        if (!std::isfinite(v)) {
            v = 0.0;
        }
        if (std::abs(v) < 1e-12) {
            v = 0.0;
        }
        ExprNode n;
        n.op = ExprOp::Const;
        n.constVal = v;
        n.isDynamic = false;
        n.isPure = true;
        return intern(n);
    }

    int makeTime()
    {
        ExprNode n;
        n.op = ExprOp::Time;
        n.isDynamic = true;
        n.isPure = true;
        return intern(n);
    }

    int makeUniformRandom(int minId, int maxId)
    {
        if (isConst(minId) && isConst(maxId) && std::abs(constVal(minId) - constVal(maxId)) <= kEps) {
            return minId;
        }
        ExprNode n;
        n.op = ExprOp::UniformRandom;
        n.args = {minId, maxId, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = true;
        n.isPure = false; // Never deduplicate random() calls
        return appendNoIntern(n);
    }

    int makeRenderAttr(const QString& attrName, int scaleId, int fallbackId)
    {
        ExprNode n;
        n.op = ExprOp::RenderAttr;
        n.attrName = attrName;
        n.args = {scaleId, fallbackId, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = true;
        n.isPure = (scaleId < 0 || node(scaleId).isPure) && node(fallbackId).isPure;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeAdd(int a, int b, bool forceLinearRamp = false)
    {
        if (isConst(a) && isConst(b)) {
            return makeConst(constVal(a) + constVal(b));
        }
        if (isZero(a) && (!forceLinearRamp || node(b).containsLinearRamp)) {
            return b;
        }
        if (isZero(b) && (!forceLinearRamp || node(a).containsLinearRamp)) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Add;
        n.args = {a, b, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure;
        n.containsLinearRamp = forceLinearRamp || node(a).containsLinearRamp || node(b).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeSub(int a, int b)
    {
        if (isConst(a) && isConst(b)) {
            return makeConst(constVal(a) - constVal(b));
        }
        if (isZero(b)) {
            return a;
        }
        if (a == b && node(a).isPure) {
            return makeConst(0.0);
        }
        ExprNode n;
        n.op = ExprOp::Sub;
        n.args = {a, b, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure;
        n.containsLinearRamp = node(a).containsLinearRamp || node(b).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeMul(int a, int b, bool forceLinearRamp = false)
    {
        if (isConst(a) && isConst(b)) {
            return makeConst(constVal(a) * constVal(b));
        }
        if ((isZero(a) && node(b).isPure) || (isZero(b) && node(a).isPure)) {
            return makeConst(0.0);
        }
        if (isOne(a) && !forceLinearRamp) {
            return b;
        }
        if (isOne(b) && !forceLinearRamp) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Mul;
        n.args = {a, b, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure;
        n.containsLinearRamp = forceLinearRamp || node(a).containsLinearRamp || node(b).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeDiv(int a, int b, bool& outDivByZero)
    {
        if (isZero(b)) {
            outDivByZero = true;
            return makeConst(0.0);
        }
        if (isConst(a) && isConst(b)) {
            return makeConst(constVal(a) / constVal(b));
        }
        if (isOne(b)) {
            return a;
        }
        if (isZero(a) && node(b).isPure) {
            return makeConst(0.0);
        }
        ExprNode n;
        n.op = ExprOp::Div;
        n.args = {a, b, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure;
        n.containsLinearRamp = node(a).containsLinearRamp || node(b).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeMod(int a, int b, bool guaranteedNonNegative, bool& outDivByZero)
    {
        if (isZero(b)) {
            outDivByZero = true;
            return makeConst(0.0);
        }
        if (isConst(a) && isConst(b)) {
            double m = std::fmod(constVal(a), constVal(b));
            if (!guaranteedNonNegative && m < 0.0) {
                m += std::abs(constVal(b));
            }
            return makeConst(m);
        }
        ExprNode n;
        n.op = guaranteedNonNegative ? ExprOp::RawMod : ExprOp::Mod;
        n.args = {a, b, -1, -1, -1};
        n.argCount = 2;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure;
        n.containsLinearRamp = false; // Bounded by modulo
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeSin(int a)
    {
        if (isConst(a)) {
            return makeConst(std::sin(constVal(a)));
        }
        ExprNode n;
        n.op = ExprOp::Sin;
        n.args = {a, -1, -1, -1, -1};
        n.argCount = 1;
        n.isDynamic = node(a).isDynamic;
        n.isPure = node(a).isPure;
        n.containsLinearRamp = false;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeCos(int a)
    {
        if (isConst(a)) {
            return makeConst(std::cos(constVal(a)));
        }
        ExprNode n;
        n.op = ExprOp::Cos;
        n.args = {a, -1, -1, -1, -1};
        n.argCount = 1;
        n.isDynamic = node(a).isDynamic;
        n.isPure = node(a).isPure;
        n.containsLinearRamp = false;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeAbs(int a)
    {
        if (isConst(a)) {
            return makeConst(std::abs(constVal(a)));
        }
        if (node(a).op == ExprOp::Abs) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Abs;
        n.args = {a, -1, -1, -1, -1};
        n.argCount = 1;
        n.isDynamic = node(a).isDynamic;
        n.isPure = node(a).isPure;
        n.containsLinearRamp = node(a).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeFrac(int a)
    {
        if (isConst(a)) {
            const double v = constVal(a);
            return makeConst(v - std::floor(v));
        }
        if (node(a).op == ExprOp::Frac) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Frac;
        n.args = {a, -1, -1, -1, -1};
        n.argCount = 1;
        n.isDynamic = node(a).isDynamic;
        n.isPure = node(a).isPure;
        n.containsLinearRamp = false;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeFloor(int a)
    {
        if (isConst(a)) {
            return makeConst(std::floor(constVal(a)));
        }
        if (node(a).op == ExprOp::Floor) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Floor;
        n.args = {a, -1, -1, -1, -1};
        n.argCount = 1;
        n.isDynamic = node(a).isDynamic;
        n.isPure = node(a).isPure;
        n.containsLinearRamp = node(a).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeClamp(int x, int minId, int maxId)
    {
        if (isConst(minId) && isConst(maxId) && constVal(minId) > constVal(maxId)) {
            std::swap(minId, maxId);
        }
        if (isConst(x) && isConst(minId) && isConst(maxId)) {
            return makeConst(std::clamp(constVal(x), constVal(minId), constVal(maxId)));
        }
        ExprNode n;
        n.op = ExprOp::Clamp;
        n.args = {x, minId, maxId, -1, -1};
        n.argCount = 3;
        n.isDynamic = node(x).isDynamic || node(minId).isDynamic || node(maxId).isDynamic;
        n.isPure = node(x).isPure && node(minId).isPure && node(maxId).isPure;
        n.containsLinearRamp = false;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeLerp(int a, int b, int t)
    {
        if (isConst(a) && isConst(b) && isConst(t)) {
            return makeConst(constVal(a) + (constVal(b) - constVal(a)) * constVal(t));
        }
        if (isZero(t) && node(b).isPure) {
            return a;
        }
        if (isOne(t) && node(a).isPure) {
            return b;
        }
        if (a == b && node(a).isPure && node(t).isPure) {
            return a;
        }
        ExprNode n;
        n.op = ExprOp::Lerp;
        n.args = {a, b, t, -1, -1};
        n.argCount = 3;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic || node(t).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure && node(t).isPure;
        n.containsLinearRamp = node(a).containsLinearRamp || node(b).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeRemapValClamped(int x, int inMin, int inMax, int outMin, int outMax)
    {
        if (isConst(x) && isConst(inMin) && isConst(inMax) && isConst(outMin) && isConst(outMax)) {
            const double span = constVal(inMax) - constVal(inMin);
            if (std::abs(span) <= kEps) {
                return outMin;
            }
            const double t = std::clamp((constVal(x) - constVal(inMin)) / span, 0.0, 1.0);
            return makeConst(constVal(outMin) + (constVal(outMax) - constVal(outMin)) * t);
        }
        ExprNode n;
        n.op = ExprOp::RemapValClamped;
        n.args = {x, inMin, inMax, outMin, outMax};
        n.argCount = 5;
        n.isDynamic = node(x).isDynamic || node(inMin).isDynamic || node(inMax).isDynamic
            || node(outMin).isDynamic || node(outMax).isDynamic;
        n.isPure = node(x).isPure && node(inMin).isPure && node(inMax).isPure
            && node(outMin).isPure && node(outMax).isPure;
        n.containsLinearRamp = false;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeLessOrEqual(int a, int b, int leVal, int gtVal)
    {
        if (isConst(a) && isConst(b)) {
            return (constVal(a) <= constVal(b)) ? leVal : gtVal;
        }
        if (leVal == gtVal && node(a).isPure && node(b).isPure) {
            return leVal;
        }
        ExprNode n;
        n.op = ExprOp::LessOrEqual;
        n.args = {a, b, leVal, gtVal, -1};
        n.argCount = 4;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic || node(leVal).isDynamic || node(gtVal).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure && node(leVal).isPure && node(gtVal).isPure;
        n.containsLinearRamp = node(leVal).containsLinearRamp || node(gtVal).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    int makeEqual(int a, int b, int eqVal, int neVal)
    {
        if (isConst(a) && isConst(b)) {
            return (std::abs(constVal(a) - constVal(b)) <= 1e-5) ? eqVal : neVal;
        }
        if (eqVal == neVal && node(a).isPure && node(b).isPure) {
            return eqVal;
        }
        ExprNode n;
        n.op = ExprOp::Equal;
        n.args = {a, b, eqVal, neVal, -1};
        n.argCount = 4;
        n.isDynamic = node(a).isDynamic || node(b).isDynamic || node(eqVal).isDynamic || node(neVal).isDynamic;
        n.isPure = node(a).isPure && node(b).isPure && node(eqVal).isPure && node(neVal).isPure;
        n.containsLinearRamp = node(eqVal).containsLinearRamp || node(neVal).containsLinearRamp;
        return n.isPure ? intern(n) : appendNoIntern(n);
    }

    // Detects pure `c * time()` (zero initial offset) for GPU native scroll fast-path.
    std::optional<double> matchPureLinearTimeVelocity(int id) const
    {
        if (id < 0) {
            return std::nullopt;
        }
        if (isZero(id)) {
            return 0.0;
        }
        const ExprNode& n = node(id);
        if (n.op == ExprOp::Time) {
            return 1.0;
        }
        if (n.op == ExprOp::Mul) {
            if (isConst(n.args[0]) && node(n.args[1]).op == ExprOp::Time) {
                return constVal(n.args[0]);
            }
            if (isConst(n.args[1]) && node(n.args[0]).op == ExprOp::Time) {
                return constVal(n.args[1]);
            }
        }
        return std::nullopt;
    }

private:
    int intern(const ExprNode& n)
    {
        ExprKey key;
        key.op = n.op;
        if (n.op == ExprOp::Const) {
            std::uint64_t bits = 0;
            static_assert(sizeof(double) == sizeof(std::uint64_t));
            std::memcpy(&bits, &n.constVal, sizeof(bits));
            key.constBits = bits;
        }
        key.attrName = n.attrName;
        key.args = n.args;
        key.argCount = n.argCount;
        key.containsLinearRamp = n.containsLinearRamp;

        const auto it = m_intern.constFind(key);
        if (it != m_intern.constEnd()) {
            return it.value();
        }
        const int id = appendNoIntern(n);
        m_intern.insert(key, id);
        return id;
    }

    int appendNoIntern(const ExprNode& n)
    {
        const int id = static_cast<int>(m_nodes.size());
        m_nodes.push_back(n);
        return id;
    }

    std::vector<ExprNode> m_nodes;
    QHash<ExprKey, int> m_intern;
};

// ============================================================================
// CSE Expression Emitter
// ============================================================================

class ExprEmitter {
public:
    explicit ExprEmitter(const ExprArena& arena) : m_arena(arena) {}

    QString emitScalar(int rootId)
    {
        return emitRoots({rootId}, 1);
    }

    QString emitVec2(int xId, int yId)
    {
        return emitRoots({xId, yId}, 2);
    }

    QString emitVec3(int xId, int yId, int zId)
    {
        return emitRoots({xId, yId, zId}, 3);
    }

private:
    bool isTrivialNode(int id) const
    {
        const ExprOp op = m_arena.node(id).op;
        return op == ExprOp::Const || op == ExprOp::Time;
    }

    void collectSubgraph(int id, QHash<int, int>& refCounts, QVector<int>& topoOrder, QSet<int>& visited) const
    {
        if (id < 0) {
            return;
        }
        refCounts[id] = refCounts.value(id, 0) + 1;
        if (visited.contains(id)) {
            return;
        }
        visited.insert(id);
        const ExprNode& n = m_arena.node(id);
        for (int i = 0; i < n.argCount; ++i) {
            if (n.args[i] >= 0) {
                collectSubgraph(n.args[i], refCounts, topoOrder, visited);
            }
        }
        topoOrder.push_back(id);
    }

    QString emitNodeExpr(int id, const QHash<int, QString>& hoisted, bool allowHoistedLookup = true) const
    {
        if (allowHoistedLookup) {
            const auto it = hoisted.constFind(id);
            if (it != hoisted.constEnd()) {
                return it.value();
            }
        }

        const ExprNode& n = m_arena.node(id);
        switch (n.op) {
        case ExprOp::Const:
            return formatExprFloat(n.constVal);
        case ExprOp::Time:
            return QStringLiteral("time()");
        case ExprOp::UniformRandom:
            return QStringLiteral("random( %1, %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::RenderAttr: {
            const QString fallbackStr = emitNodeExpr(n.args[1], hoisted);
            if (n.args[0] >= 0) {
                const QString scaleStr = emitNodeExpr(n.args[0], hoisted);
                return QStringLiteral("( exists( %1 ) ? ( %1 * %2 ) : %3 )")
                    .arg(n.attrName, scaleStr, fallbackStr);
            }
            return QStringLiteral("( exists( %1 ) ? %1 : %2 )").arg(n.attrName, fallbackStr);
        }
        case ExprOp::Add:
            return QStringLiteral("( %1 + %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Sub:
            return QStringLiteral("( %1 - %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Mul:
            return QStringLiteral("( %1 * %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Div:
            return QStringLiteral("( %1 / %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::RawMod:
            return QStringLiteral("( %1 % %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Mod: {
            const QString a = emitNodeExpr(n.args[0], hoisted);
            const QString b = emitNodeExpr(n.args[1], hoisted);
            return QStringLiteral("( ( ( %1 % %2 ) + %2 ) % %2 )").arg(a, b);
        }
        case ExprOp::Neg:
            return QStringLiteral("( -%1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Sin:
            return QStringLiteral("sin( %1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Cos:
            return QStringLiteral("cos( %1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Abs:
            return QStringLiteral("abs( %1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Frac:
            return QStringLiteral("frac( %1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Floor:
            return QStringLiteral("floor( %1 )").arg(emitNodeExpr(n.args[0], hoisted));
        case ExprOp::Min:
            return QStringLiteral("min( %1, %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Max:
            return QStringLiteral("max( %1, %2 )")
                .arg(emitNodeExpr(n.args[0], hoisted), emitNodeExpr(n.args[1], hoisted));
        case ExprOp::Clamp:
            return QStringLiteral("clamp( %1, %2, %3 )")
                .arg(emitNodeExpr(n.args[0], hoisted),
                     emitNodeExpr(n.args[1], hoisted),
                     emitNodeExpr(n.args[2], hoisted));
        case ExprOp::Lerp:
            return QStringLiteral("lerp( %1, %2, %3 )")
                .arg(emitNodeExpr(n.args[0], hoisted),
                     emitNodeExpr(n.args[1], hoisted),
                     emitNodeExpr(n.args[2], hoisted));
        case ExprOp::RemapValClamped:
            return QStringLiteral("RemapValClamped( %1, %2, %3, %4, %5 )")
                .arg(emitNodeExpr(n.args[0], hoisted),
                     emitNodeExpr(n.args[1], hoisted),
                     emitNodeExpr(n.args[2], hoisted),
                     emitNodeExpr(n.args[3], hoisted),
                     emitNodeExpr(n.args[4], hoisted));
        case ExprOp::LessOrEqual:
            return QStringLiteral("( ( %1 <= %2 ) ? %3 : %4 )")
                .arg(emitNodeExpr(n.args[0], hoisted),
                     emitNodeExpr(n.args[1], hoisted),
                     emitNodeExpr(n.args[2], hoisted),
                     emitNodeExpr(n.args[3], hoisted));
        case ExprOp::Equal:
            return QStringLiteral("( ( %1 == %2 ) ? %3 : %4 )")
                .arg(emitNodeExpr(n.args[0], hoisted),
                     emitNodeExpr(n.args[1], hoisted),
                     emitNodeExpr(n.args[2], hoisted),
                     emitNodeExpr(n.args[3], hoisted));
        }
        return QStringLiteral("0.0");
    }

    QString emitRoots(const std::vector<int>& roots, int width) const
    {
        QHash<int, int> refCounts;
        QVector<int> topoOrder;
        QSet<int> visited;
        for (const int rootId : roots) {
            collectSubgraph(rootId, refCounts, topoOrder, visited);
        }

        QHash<int, QString> hoisted;
        QStringList statements;
        int varIndex = 0;

        for (const int id : topoOrder) {
            if (refCounts.value(id, 0) >= 2 && !isTrivialNode(id)) {
                const QString rhs = emitNodeExpr(id, hoisted, false);
                const QString varName = QStringLiteral("v%1").arg(varIndex++);
                statements.push_back(QStringLiteral("%1 = %2;").arg(varName, rhs));
                hoisted.insert(id, varName);
            }
        }

        QString finalExpr;
        if (width == 1) {
            finalExpr = emitNodeExpr(roots[0], hoisted);
        } else if (width == 2) {
            finalExpr = QStringLiteral("float2( %1, %2 )")
                .arg(emitNodeExpr(roots[0], hoisted), emitNodeExpr(roots[1], hoisted));
        } else {
            finalExpr = QStringLiteral("float3( %1, %2, %3 )")
                .arg(emitNodeExpr(roots[0], hoisted),
                     emitNodeExpr(roots[1], hoisted),
                     emitNodeExpr(roots[2], hoisted));
        }

        if (statements.isEmpty()) {
            return finalExpr;
        }
        statements.push_back(QStringLiteral("return %1;").arg(finalExpr));
        return statements.join(u' ');
    }

    const ExprArena& m_arena;
};

// ============================================================================
// Symbolic Environment & Transform State
// ============================================================================

struct SymValue {
    int width = 1;
    std::array<int, 4> comp = {-1, -1, -1, -1};
    std::array<bool, 4> tainted = {false, false, false, false};
    bool modifiedByProxy = false;

    bool anyTainted() const noexcept
    {
        for (int i = 0; i < width; ++i) {
            if (tainted[i]) {
                return true;
            }
        }
        return false;
    }

    bool anyDynamic(const ExprArena& arena) const
    {
        for (int i = 0; i < width; ++i) {
            if (comp[i] >= 0 && arena.node(comp[i]).isDynamic) {
                return true;
            }
        }
        return false;
    }
};

struct TransformState {
    std::array<int, 2> center = {-1, -1};
    std::array<int, 2> scale = {-1, -1};
    int rotate = -1;
    std::array<int, 2> translate = {-1, -1};

    bool centerModified = false;
    bool scaleModified = false;
    bool rotateModified = false;
    bool translateModified = false;
    bool modifiedByProxy = false;
    bool tainted = false;

    bool isPureConstantScroll = false;
    double scrollSpeedU = 0.0;
    double scrollSpeedV = 0.0;
};

// ============================================================================
// Shader Capability Table (§19.2)
// ============================================================================

bool supportsTextureAnimation(TargetShader shader) noexcept
{
    return shader == TargetShader::CsgoComplex || shader == TargetShader::CsgoStaticOverlay;
}

bool supportsNativeScrollSlot(TargetShader shader, const QString& slotName)
{
    switch (shader) {
    case TargetShader::CsgoComplex:
        return slotName == QLatin1String("g_vTexCoordScrollSpeed")
            || slotName == QLatin1String("g_vSelfIllumScrollSpeed");
    case TargetShader::CsgoStaticOverlay:
    case TargetShader::CsgoFoliage:
        return slotName == QLatin1String("g_vTexCoordScrollSpeed");
    case TargetShader::CsgoEffects:
        return slotName == QLatin1String("g_vTexCoordScrollSpeed")
            || slotName == QLatin1String("g_vMask1PanSpeed")
            || slotName == QLatin1String("g_vMask2PanSpeed")
            || slotName == QLatin1String("g_vMask3PanSpeed");
    case TargetShader::CsgoWaterFancy:
        return slotName == QLatin1String("g_vTexCoordScrollSpeed")
            || slotName == QLatin1String("g_flWavesSpeed");
    default:
        return false;
    }
}

bool isDynamicParamAllowed(TargetShader shader, const QString& paramName)
{
    // Hard compiler guard (§19.3): NEVER allow F_* macros or g_n* int params in DynamicParams
    if (paramName.startsWith(QLatin1String("F_")) || paramName.startsWith(QLatin1String("g_n"))) {
        return false;
    }

    switch (shader) {
    case TargetShader::CsgoComplex:
        return paramName == QLatin1String("g_vTexCoordOffset")
            || paramName == QLatin1String("g_vTexCoordScale")
            || paramName == QLatin1String("g_flTexCoordRotation")
            || paramName == QLatin1String("g_vTexCoordCenter")
            || paramName == QLatin1String("g_vDetailTexCoordOffset")
            || paramName == QLatin1String("g_vDetailTexCoordScale")
            || paramName == QLatin1String("g_flDetailTexCoordRotation")
            || paramName == QLatin1String("g_vDetailTexCoordCenter")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flOpacityScale")
            || paramName == QLatin1String("g_flSelfIllumBrightness")
            || paramName == QLatin1String("g_flSelfIllumScale")
            || paramName == QLatin1String("g_vSelfIllumTint")
            || paramName == QLatin1String("g_flDetailBlendFactor")
            || paramName == QLatin1String("g_flAnimationFrame");
    case TargetShader::CsgoEnvironment:
        return paramName == QLatin1String("g_vTexCoordOffset1")
            || paramName == QLatin1String("g_vTexCoordScale1")
            || paramName == QLatin1String("g_flTexCoordRotation1")
            || paramName == QLatin1String("g_vTexCoordCenter1")
            || paramName == QLatin1String("g_vDetailTexCoordOffset1")
            || paramName == QLatin1String("g_vDetailTexCoordScale1")
            || paramName == QLatin1String("g_flDetailTexCoordRotation1")
            || paramName == QLatin1String("g_vDetailTexCoordCenter1")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_vTextureColorTint1")
            || paramName == QLatin1String("g_fTextureColorBrightness1")
            || paramName == QLatin1String("g_fTextureColorContrast1")
            || paramName == QLatin1String("g_fTextureColorSaturation1");
    case TargetShader::CsgoEnvironmentBlend:
        return paramName == QLatin1String("g_vTexCoordOffset1")
            || paramName == QLatin1String("g_vTexCoordOffset2")
            || paramName == QLatin1String("g_vTexCoordOffset3")
            || paramName == QLatin1String("g_vTexCoordScale1")
            || paramName == QLatin1String("g_vTexCoordScale2")
            || paramName == QLatin1String("g_vTexCoordScale3")
            || paramName == QLatin1String("g_flTexCoordRotation1")
            || paramName == QLatin1String("g_flTexCoordRotation2")
            || paramName == QLatin1String("g_flTexCoordRotation3")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_vTextureColorTint1")
            || paramName == QLatin1String("g_vTextureColorTint2")
            || paramName == QLatin1String("g_vTextureColorTint3");
    case TargetShader::CsgoStaticOverlay:
        return paramName == QLatin1String("g_vTexCoordOffset")
            || paramName == QLatin1String("g_vTexCoordScale")
            || paramName == QLatin1String("g_flTexCoordRotation")
            || paramName == QLatin1String("g_vTexCoordCenter")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flOpacityScale")
            || paramName == QLatin1String("g_flSelfIllumBrightness")
            || paramName == QLatin1String("g_flSelfIllumScale")
            || paramName == QLatin1String("g_vSelfIllumTint")
            || paramName == QLatin1String("g_flAnimationFrame");
    case TargetShader::CsgoEffects:
        return paramName == QLatin1String("g_vMask1Scale")
            || paramName == QLatin1String("g_vMask2Scale")
            || paramName == QLatin1String("g_vMask3Scale")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flColorBoost")
            || paramName == QLatin1String("g_flOpacityScale");
    case TargetShader::CsgoWaterFancy:
        return paramName == QLatin1String("g_vTexCoordOffset")
            || paramName == QLatin1String("g_vTexCoordScale")
            || paramName == QLatin1String("g_flTexCoordRotation")
            || paramName == QLatin1String("g_vTexCoordCenter")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flWavesSpeed")
            || paramName == QLatin1String("g_flWavesNormalStrength");
    case TargetShader::CsgoRefract:
        return paramName == QLatin1String("g_vTexCoordOffset")
            || paramName == QLatin1String("g_vTexCoordScale")
            || paramName == QLatin1String("g_flTexCoordRotation")
            || paramName == QLatin1String("g_vSecondaryTexCoordScale")
            || paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flRefractScale");
    case TargetShader::CsgoFoliage:
        return paramName == QLatin1String("g_vColorTint")
            || paramName == QLatin1String("g_flSwaySpeed")
            || paramName == QLatin1String("g_flFlutterSpeed");
    case TargetShader::CsgoGlass:
        return paramName == QLatin1String("g_flBreakTime")
            || paramName == QLatin1String("g_flBreakBounceFloor")
            || paramName == QLatin1String("g_vBreakShotPosition");
    default:
        return false;
    }
}

bool isStaticParamAllowed(TargetShader shader, const QString& paramName)
{
    return isDynamicParamAllowed(shader, paramName) || supportsNativeScrollSlot(shader, paramName);
}

// ============================================================================
// Compiler Session (Single Cohesive Pass)
// ============================================================================

class ProxyCompileSession {
public:
    ProxyCompileSession(
        const VmtMaterial& vmt,
        TargetShader targetShader,
        const std::optional<TextureAtlasFrameInfo>& atlasInfo)
        : m_vmt(vmt)
        , m_targetShader(targetShader)
        , m_atlasInfo(atlasInfo)
    {
        seedEnvironment();
    }

    MaterialAnimationPlan run()
    {
        evaluateProxies();
        lowerToPlan();
        return m_plan;
    }

private:
    void addWarning(const QString& msg)
    {
        if (!m_plan.warnings.contains(msg)) {
            m_plan.warnings.push_back(msg);
        }
    }

    SymValue makeScalar(int nodeId, bool tainted = false) const
    {
        SymValue v;
        v.width = 1;
        v.comp = {nodeId, nodeId, nodeId, nodeId};
        v.tainted = {tainted, tainted, tainted, tainted};
        return v;
    }

    SymValue makeConstScalar(double val)
    {
        return makeScalar(m_arena.makeConst(val), false);
    }

    SymValue makeDefaultForVar(const QString& baseName)
    {
        if (isColorLikeVar(baseName)) {
            const int one = m_arena.makeConst(1.0);
            SymValue v;
            v.width = 3;
            v.comp = {one, one, one, one};
            return v;
        }
        if (baseName == QLatin1String("$alpha")) {
            return makeConstScalar(1.0);
        }
        return makeConstScalar(0.0);
    }

    TransformState makeIdentityTransform()
    {
        TransformState ts;
        const int half = m_arena.makeConst(0.5);
        const int one = m_arena.makeConst(1.0);
        const int zero = m_arena.makeConst(0.0);
        ts.center = {half, half};
        ts.scale = {one, one};
        ts.rotate = zero;
        ts.translate = {zero, zero};
        return ts;
    }

    TransformState& getOrCreateTransform(const QString& varName)
    {
        const QString key = varName.trimmed().toLower();
        auto it = m_transforms.find(key);
        if (it == m_transforms.end()) {
            it = m_transforms.insert(key, makeIdentityTransform());
        }
        return it.value();
    }

    void seedEnvironment()
    {
        for (auto it = m_vmt.params.constBegin(); it != m_vmt.params.constEnd(); ++it) {
            const QString& key = it.key();
            const QString& val = it.value();

            if (isTransformVar(key)) {
                if (const auto parsed = VmtParser::parseTransform(val); parsed.has_value()) {
                    TransformState ts;
                    ts.center = {m_arena.makeConst(parsed->center[0]), m_arena.makeConst(parsed->center[1])};
                    ts.scale = {m_arena.makeConst(parsed->scale[0]), m_arena.makeConst(parsed->scale[1])};
                    ts.rotate = m_arena.makeConst(parsed->rotateDeg);
                    ts.translate = {m_arena.makeConst(parsed->translate[0]), m_arena.makeConst(parsed->translate[1])};
                    m_transforms.insert(key, ts);
                }
                continue;
            }

            if (const auto lit = parseLiteral(val); lit.has_value()) {
                SymValue sv;
                if (isColorLikeVar(key) && lit->width == 1) {
                    const int c = m_arena.makeConst(lit->values[0]);
                    sv.width = 3;
                    sv.comp = {c, c, c, m_arena.makeConst(1.0)};
                } else {
                    sv.width = lit->width;
                    for (int i = 0; i < sv.width; ++i) {
                        sv.comp[i] = m_arena.makeConst(lit->values[i]);
                    }
                    for (int i = sv.width; i < 4; ++i) {
                        sv.comp[i] = sv.comp[sv.width - 1];
                    }
                }
                m_env.insert(key, sv);
            }
        }
    }

    SymValue resolveOperand(const QString& raw, double defaultScalar = 0.0)
    {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) {
            return makeConstScalar(defaultScalar);
        }

        if (const auto ref = parseVarRef(trimmed); ref.has_value()) {
            auto it = m_env.find(ref->baseName);
            if (it == m_env.end()) {
                it = m_env.insert(ref->baseName, makeDefaultForVar(ref->baseName));
            }
            const SymValue& sv = it.value();
            if (ref->component >= 0) {
                const int idx = ref->component;
                const int nodeId = (idx < sv.width && sv.comp[idx] >= 0)
                    ? sv.comp[idx]
                    : m_arena.makeConst(0.0);
                const bool t = (idx < sv.width) ? sv.tainted[idx] : false;
                return makeScalar(nodeId, t);
            }
            return sv;
        }

        if (const auto lit = parseLiteral(trimmed); lit.has_value()) {
            SymValue sv;
            sv.width = lit->width;
            for (int i = 0; i < sv.width; ++i) {
                sv.comp[i] = m_arena.makeConst(lit->values[i]);
            }
            for (int i = sv.width; i < 4; ++i) {
                sv.comp[i] = sv.comp[sv.width - 1];
            }
            return sv;
        }

        return makeConstScalar(defaultScalar);
    }

    int resolveScalarNode(const QString& raw, double defaultScalar, bool& outTainted)
    {
        const SymValue sv = resolveOperand(raw, defaultScalar);
        if (sv.tainted[0]) {
            outTainted = true;
        }
        return sv.comp[0] >= 0 ? sv.comp[0] : m_arena.makeConst(defaultScalar);
    }

    void writeTarget(const QString& rawTarget, const SymValue& rhs)
    {
        const auto ref = parseVarRef(rawTarget);
        if (!ref.has_value()) {
            return;
        }

        auto it = m_env.find(ref->baseName);
        if (it == m_env.end()) {
            it = m_env.insert(ref->baseName, makeDefaultForVar(ref->baseName));
        }
        SymValue& dst = it.value();
        dst.modifiedByProxy = true;

        if (ref->component >= 0) {
            const int idx = ref->component;
            const int minWidth = std::max(2, idx + 1);
            if (minWidth > dst.width) {
                const int zero = m_arena.makeConst(0.0);
                for (int i = dst.width; i < minWidth; ++i) {
                    dst.comp[i] = zero;
                    dst.tainted[i] = false;
                }
                dst.width = minWidth;
            }
            dst.comp[idx] = rhs.comp[0];
            dst.tainted[idx] = rhs.tainted[0];
            return;
        }

        // Whole-variable write: broadcast scalar to vector if destination is color-like or already wider
        const int targetWidth = isColorLikeVar(ref->baseName)
            ? 3
            : std::max(dst.width, rhs.width);
        dst.width = targetWidth;
        for (int i = 0; i < targetWidth; ++i) {
            const int srcIdx = (i < rhs.width) ? i : 0;
            dst.comp[i] = rhs.comp[srcIdx];
            dst.tainted[i] = rhs.tainted[srcIdx];
        }
    }

    void taintTarget(const QString& rawTarget)
    {
        if (rawTarget.trimmed().isEmpty()) {
            return;
        }
        const QString lower = rawTarget.trimmed().toLower();
        if (isTransformVar(lower)) {
            TransformState& ts = getOrCreateTransform(lower);
            ts.tainted = true;
            return;
        }
        const auto ref = parseVarRef(rawTarget);
        if (!ref.has_value()) {
            return;
        }
        auto it = m_env.find(ref->baseName);
        if (it == m_env.end()) {
            it = m_env.insert(ref->baseName, makeDefaultForVar(ref->baseName));
        }
        SymValue& dst = it.value();
        dst.modifiedByProxy = true;
        if (ref->component >= 0) {
            const int idx = ref->component;
            if (idx >= dst.width) {
                const int zero = m_arena.makeConst(0.0);
                for (int i = dst.width; i <= idx; ++i) {
                    dst.comp[i] = zero;
                    dst.tainted[i] = false;
                }
                dst.width = idx + 1;
            }
            dst.tainted[idx] = true;
        } else {
            for (int i = 0; i < dst.width; ++i) {
                dst.tainted[i] = true;
            }
        }
    }

    template <typename Fn>
    SymValue applyUnary(const SymValue& a, Fn&& fn)
    {
        SymValue out;
        out.width = a.width;
        for (int i = 0; i < a.width; ++i) {
            out.comp[i] = fn(a.comp[i]);
            out.tainted[i] = a.tainted[i];
        }
        return out;
    }

    template <typename Fn>
    SymValue applyBinary(const SymValue& a, const SymValue& b, Fn&& fn)
    {
        SymValue out;
        out.width = std::max(a.width, b.width);
        for (int i = 0; i < out.width; ++i) {
            const int ia = (i < a.width) ? i : 0;
            const int ib = (i < b.width) ? i : 0;
            out.comp[i] = fn(a.comp[ia], b.comp[ib]);
            out.tainted[i] = a.tainted[ia] || b.tainted[ib];
        }
        return out;
    }

    void evaluateProxies()
    {
        for (const auto& proxy : m_vmt.proxies) {
            const QString& name = proxy.name;

            // ----------------------------------------------------------------
            // Tier A: TextureScroll
            // ----------------------------------------------------------------
            if (name == QLatin1String("texturescroll")) {
                const QString targetVar = proxy.param(QStringLiteral("texturescrollvar"), QStringLiteral("$basetexturetransform"))
                                              .trimmed().toLower();
                TransformState& ts = getOrCreateTransform(targetVar);
                ts.modifiedByProxy = true;

                bool tainted = false;
                const int rateId = resolveScalarNode(proxy.param(QStringLiteral("texturescrollrate")), 1.0, tainted);
                const int angleDegId = resolveScalarNode(proxy.param(QStringLiteral("texturescrollangle")), 0.0, tainted);
                const int offsetYId = resolveScalarNode(proxy.param(QStringLiteral("texturescrolloffsety")), 0.0, tainted);

                if (proxy.hasParam(QStringLiteral("texturescale"))) {
                    const int scaleId = resolveScalarNode(proxy.param(QStringLiteral("texturescale")), 1.0, tainted);
                    ts.scale = {scaleId, scaleId};
                    ts.scaleModified = true;
                }

                if (tainted) {
                    ts.tainted = true;
                    continue;
                }

                const int degToRadId = m_arena.makeConst(kDegToRad);
                const int angleRadId = m_arena.makeMul(angleDegId, degToRadId);
                const int uSpeedId = m_arena.makeMul(rateId, m_arena.makeCos(angleRadId));
                const int vSpeedId = m_arena.makeMul(rateId, m_arena.makeSin(angleRadId));
                const int timeId = m_arena.makeTime();

                const int uOffId = m_arena.makeMul(uSpeedId, timeId, true);
                const int vOffId = m_arena.makeAdd(m_arena.makeMul(vSpeedId, timeId, true), offsetYId, true);

                ts.translate = {uOffId, vOffId};
                ts.translateModified = true;

                if (m_arena.isConst(uSpeedId) && m_arena.isConst(vSpeedId) && m_arena.isZero(offsetYId) && m_arena.isZero(ts.rotate)) {
                    ts.isPureConstantScroll = true;
                    ts.scrollSpeedU = m_arena.constVal(uSpeedId);
                    ts.scrollSpeedV = m_arena.constVal(vSpeedId);
                } else {
                    ts.isPureConstantScroll = false;
                }
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: TextureTransform
            // ----------------------------------------------------------------
            if (name == QLatin1String("texturetransform")) {
                const QString targetVar = proxy.param(QStringLiteral("resultvar"), QStringLiteral("$basetexturetransform"))
                                              .trimmed().toLower();
                TransformState& ts = getOrCreateTransform(targetVar);
                ts.modifiedByProxy = true;
                ts.isPureConstantScroll = false;

                if (proxy.hasParam(QStringLiteral("centervar"))) {
                    const SymValue cv = resolveOperand(proxy.param(QStringLiteral("centervar")), 0.5);
                    if (cv.anyTainted()) {
                        ts.tainted = true;
                    } else {
                        ts.center = {cv.comp[0], cv.width >= 2 ? cv.comp[1] : cv.comp[0]};
                        ts.centerModified = true;
                    }
                }
                if (proxy.hasParam(QStringLiteral("scalevar"))) {
                    const SymValue sv = resolveOperand(proxy.param(QStringLiteral("scalevar")), 1.0);
                    if (sv.anyTainted()) {
                        ts.tainted = true;
                    } else {
                        ts.scale = {sv.comp[0], sv.width >= 2 ? sv.comp[1] : sv.comp[0]};
                        ts.scaleModified = true;
                    }
                }
                if (proxy.hasParam(QStringLiteral("rotatevar"))) {
                    bool t = false;
                    const int rv = resolveScalarNode(proxy.param(QStringLiteral("rotatevar")), 0.0, t);
                    if (t) {
                        ts.tainted = true;
                    } else {
                        ts.rotate = rv;
                        ts.rotateModified = true;
                    }
                }
                if (proxy.hasParam(QStringLiteral("translatevar"))) {
                    const SymValue tv = resolveOperand(proxy.param(QStringLiteral("translatevar")), 0.0);
                    if (tv.anyTainted()) {
                        ts.tainted = true;
                    } else {
                        ts.translate = {tv.comp[0], tv.width >= 2 ? tv.comp[1] : tv.comp[0]};
                        ts.translateModified = true;
                    }
                }
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: AnimatedTexture
            // ----------------------------------------------------------------
            if (name == QLatin1String("animatedtexture")
                || name == QLatin1String("animatedentitytexture")
                || name == QLatin1String("animatedoffsettexture")) {
                const QString frameVar = proxy.param(QStringLiteral("animatedtextureframenumvar"), QStringLiteral("$frame"))
                                             .trimmed().toLower();
                bool tainted = false;
                const int fpsId = resolveScalarNode(proxy.param(QStringLiteral("animatedtextureframerate")), 10.0, tainted);

                m_hasAnimatedTexture = true;
                m_animatedFrameVar = frameVar;

                const int cellCount = (m_atlasInfo.has_value() && m_atlasInfo->cellCount > 0)
                    ? m_atlasInfo->cellCount
                    : 1;

                if (tainted) {
                    taintTarget(frameVar);
                    continue;
                }

                if (m_arena.isConst(fpsId) && m_arena.constVal(fpsId) > kEps) {
                    m_animatedFps = static_cast<float>(m_arena.constVal(fpsId));
                    m_animatedFpsIsConst = true;
                } else {
                    m_animatedFpsIsConst = false;
                }

                const int scaledTime = m_arena.makeMul(m_arena.makeTime(), fpsId);
                const int floored = m_arena.makeFloor(scaledTime);
                bool divZero = false;
                const int frameExpr = (cellCount > 1)
                    ? m_arena.makeMod(floored, m_arena.makeConst(static_cast<double>(cellCount)), true, divZero)
                    : floored;

                m_animatedFrameInitialNodeId = frameExpr;
                writeTarget(frameVar, makeScalar(frameExpr, false));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: Sine
            // ----------------------------------------------------------------
            if (name == QLatin1String("sine")) {
                bool tainted = false;
                int periodId = resolveScalarNode(proxy.param(QStringLiteral("sineperiod")), 1.0, tainted);
                const int minId = resolveScalarNode(proxy.param(QStringLiteral("sinemin")), 0.0, tainted);
                const int maxId = resolveScalarNode(proxy.param(QStringLiteral("sinemax")), 1.0, tainted);
                const QString offsetStr = proxy.hasParam(QStringLiteral("timeoffset"))
                    ? proxy.param(QStringLiteral("timeoffset"))
                    : proxy.param(QStringLiteral("sinetimeoffset"));
                const int offsetId = resolveScalarNode(offsetStr, 0.0, tainted);
                const QString resultVar = proxy.param(QStringLiteral("resultvar"));

                if (tainted) {
                    taintTarget(resultVar);
                    continue;
                }

                if (m_arena.isZero(periodId)) {
                    periodId = m_arena.makeConst(1.0);
                }

                bool divZero = false;
                const int freq = m_arena.makeDiv(m_arena.makeConst(kTwoPi), periodId, divZero);
                const int phase = m_arena.makeMul(freq, m_arena.makeSub(m_arena.makeTime(), offsetId));
                const int half = m_arena.makeConst(0.5);
                const int wave01 = m_arena.makeAdd(m_arena.makeMul(half, m_arena.makeSin(phase)), half);
                const int val = m_arena.makeLerp(minId, maxId, wave01);
                writeTarget(resultVar, makeScalar(val, false));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: LinearRamp
            // ----------------------------------------------------------------
            if (name == QLatin1String("linearramp")) {
                bool tainted = false;
                const int rateId = resolveScalarNode(proxy.param(QStringLiteral("rate")), 1.0, tainted);
                const int initId = resolveScalarNode(proxy.param(QStringLiteral("initialvalue")), 0.0, tainted);
                const QString resultVar = proxy.param(QStringLiteral("resultvar"));

                if (tainted) {
                    taintTarget(resultVar);
                    continue;
                }

                const int ramp = m_arena.makeAdd(initId, m_arena.makeMul(rateId, m_arena.makeTime(), true), true);
                writeTarget(resultVar, makeScalar(ramp, false));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: CurrentTime
            // ----------------------------------------------------------------
            if (name == QLatin1String("currenttime")) {
                writeTarget(proxy.param(QStringLiteral("resultvar")), makeScalar(m_arena.makeTime(), false));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier A: Arithmetic & Math
            // ----------------------------------------------------------------
            if (name == QLatin1String("add")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue b = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(a, b, [&](int x, int y) { return m_arena.makeAdd(x, y); }));
                continue;
            }
            if (name == QLatin1String("subtract")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue b = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(a, b, [&](int x, int y) { return m_arena.makeSub(x, y); }));
                continue;
            }
            if (name == QLatin1String("multiply")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 1.0);
                const SymValue b = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 1.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(a, b, [&](int x, int y) { return m_arena.makeMul(x, y); }));
                continue;
            }
            if (name == QLatin1String("divide")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue b = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 1.0);
                bool divZero = false;
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(a, b, [&](int x, int y) { return m_arena.makeDiv(x, y, divZero); }));
                if (divZero) {
                    addWarning(QCoreApplication::translate(
                        "MaterialProxyCompiler",
                        "Division by zero in Divide proxy guarded to 0.0"));
                }
                continue;
            }
            if (name == QLatin1String("modulo")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue b = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 1.0);
                bool divZero = false;
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(a, b, [&](int x, int y) { return m_arena.makeMod(x, y, false, divZero); }));
                if (divZero) {
                    addWarning(QCoreApplication::translate(
                        "MaterialProxyCompiler",
                        "Modulo by zero in Modulo proxy guarded to 0.0"));
                }
                continue;
            }
            if (name == QLatin1String("frac")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyUnary(a, [&](int x) { return m_arena.makeFrac(x); }));
                continue;
            }
            if (name == QLatin1String("abs")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyUnary(a, [&](int x) { return m_arena.makeAbs(x); }));
                continue;
            }
            if (name == QLatin1String("clamp")) {
                const SymValue x = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                bool t = false;
                const int minId = resolveScalarNode(proxy.param(QStringLiteral("min")), 0.0, t);
                const int maxId = resolveScalarNode(proxy.param(QStringLiteral("max")), 1.0, t);
                if (t) {
                    taintTarget(proxy.param(QStringLiteral("resultvar")));
                } else {
                    writeTarget(proxy.param(QStringLiteral("resultvar")),
                        applyUnary(x, [&](int v) { return m_arena.makeClamp(v, minId, maxId); }));
                }
                continue;
            }
            if (name == QLatin1String("equals")) {
                const SymValue a = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")), a);
                continue;
            }
            if (name == QLatin1String("lessorequal")) {
                const SymValue s1 = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue s2 = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 0.0);
                const SymValue le = resolveOperand(proxy.param(QStringLiteral("lessequalvar")), 0.0);
                const SymValue gt = resolveOperand(proxy.param(QStringLiteral("greatervar")), 0.0);
                SymValue out;
                out.width = std::max({s1.width, s2.width, le.width, gt.width});
                for (int i = 0; i < out.width; ++i) {
                    const int i1 = (i < s1.width) ? i : 0;
                    const int i2 = (i < s2.width) ? i : 0;
                    const int il = (i < le.width) ? i : 0;
                    const int ig = (i < gt.width) ? i : 0;
                    out.comp[i] = m_arena.makeLessOrEqual(s1.comp[i1], s2.comp[i2], le.comp[il], gt.comp[ig]);
                    out.tainted[i] = s1.tainted[i1] || s2.tainted[i2] || le.tainted[il] || gt.tainted[ig];
                }
                writeTarget(proxy.param(QStringLiteral("resultvar")), out);
                continue;
            }
            if (name == QLatin1String("remapvalclamp")) {
                const SymValue x = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                bool t = false;
                const QString inMinStr = proxy.hasParam(QStringLiteral("range_in_min"))
                    ? proxy.param(QStringLiteral("range_in_min"))
                    : proxy.param(QStringLiteral("inmin"));
                const QString inMaxStr = proxy.hasParam(QStringLiteral("range_in_max"))
                    ? proxy.param(QStringLiteral("range_in_max"))
                    : proxy.param(QStringLiteral("inmax"));
                const QString outMinStr = proxy.hasParam(QStringLiteral("range_out_min"))
                    ? proxy.param(QStringLiteral("range_out_min"))
                    : proxy.param(QStringLiteral("outmin"));
                const QString outMaxStr = proxy.hasParam(QStringLiteral("range_out_max"))
                    ? proxy.param(QStringLiteral("range_out_max"))
                    : proxy.param(QStringLiteral("outmax"));
                const int inMin = resolveScalarNode(inMinStr, 0.0, t);
                const int inMax = resolveScalarNode(inMaxStr, 1.0, t);
                const int outMin = resolveScalarNode(outMinStr, 0.0, t);
                const int outMax = resolveScalarNode(outMaxStr, 1.0, t);
                if (t) {
                    taintTarget(proxy.param(QStringLiteral("resultvar")));
                } else {
                    writeTarget(proxy.param(QStringLiteral("resultvar")),
                        applyUnary(x, [&](int v) { return m_arena.makeRemapValClamped(v, inMin, inMax, outMin, outMax); }));
                }
                continue;
            }
            if (name == QLatin1String("select")) {
                bool t = false;
                const int selId = resolveScalarNode(proxy.param(QStringLiteral("srcvar1")), 0.0, t);
                SymValue acc = resolveOperand(proxy.param(QStringLiteral("default")), 0.0);

                // Collect selection0 .. selectionN
                QVector<QPair<int, QString>> selections;
                for (auto it = proxy.params.constBegin(); it != proxy.params.constEnd(); ++it) {
                    if (it.key().startsWith(QLatin1String("selection"))) {
                        bool ok = false;
                        const int idx = it.key().mid(9).toInt(&ok);
                        if (ok && idx >= 0) {
                            selections.push_back({idx, it.value()});
                        }
                    }
                }
                std::sort(selections.begin(), selections.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });

                if (t) {
                    taintTarget(proxy.param(QStringLiteral("resultvar")));
                } else {
                    // Use floor(sel + 0.5) instead of forbidden round()
                    const int roundedSel = m_arena.makeFloor(m_arena.makeAdd(selId, m_arena.makeConst(0.5)));
                    for (int s = static_cast<int>(selections.size()) - 1; s >= 0; --s) {
                        const int idxVal = m_arena.makeConst(static_cast<double>(selections[s].first));
                        const SymValue branch = resolveOperand(selections[s].second, 0.0);
                        SymValue next;
                        next.width = std::max(acc.width, branch.width);
                        for (int c = 0; c < next.width; ++c) {
                            const int ib = (c < branch.width) ? c : 0;
                            const int ia = (c < acc.width) ? c : 0;
                            next.comp[c] = m_arena.makeEqual(roundedSel, idxVal, branch.comp[ib], acc.comp[ia]);
                            next.tainted[c] = branch.tainted[ib] || acc.tainted[ia];
                        }
                        acc = next;
                    }
                    writeTarget(proxy.param(QStringLiteral("resultvar")), acc);
                }
                continue;
            }
            if (name == QLatin1String("selectfirstifnonzero")) {
                const SymValue s1 = resolveOperand(proxy.param(QStringLiteral("srcvar1")), 0.0);
                const SymValue s2 = resolveOperand(proxy.param(QStringLiteral("srcvar2")), 0.0);
                const int zero = m_arena.makeConst(0.0);
                writeTarget(proxy.param(QStringLiteral("resultvar")),
                    applyBinary(s1, s2, [&](int x, int y) { return m_arena.makeEqual(x, zero, y, x); }));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier B: UniformNoise, GaussianNoise, EntityRandom, WaterLOD, BreakableSurface
            // ----------------------------------------------------------------
            if (name == QLatin1String("uniformnoise")) {
                bool t = false;
                const int minId = resolveScalarNode(proxy.param(QStringLiteral("minval")), 0.0, t);
                const int maxId = resolveScalarNode(proxy.param(QStringLiteral("maxval")), 1.0, t);
                const QString resultVar = proxy.param(QStringLiteral("resultvar"));
                if (t) {
                    taintTarget(resultVar);
                } else {
                    writeTarget(resultVar, makeScalar(m_arena.makeUniformRandom(minId, maxId), false));
                }
                continue;
            }
            if (name == QLatin1String("gaussiannoise")) {
                bool t = false;
                const int meanId = resolveScalarNode(proxy.param(QStringLiteral("mean")), 0.0, t);
                const int halfWidthId = resolveScalarNode(proxy.param(QStringLiteral("halfwidth")), 1.0, t);
                const QString resultVar = proxy.param(QStringLiteral("resultvar"));
                if (t) {
                    taintTarget(resultVar);
                    continue;
                }
                const int negOne = m_arena.makeConst(-1.0);
                const int posOne = m_arena.makeConst(1.0);
                const int r1 = m_arena.makeUniformRandom(negOne, posOne);
                const int r2 = m_arena.makeUniformRandom(negOne, posOne);
                const int r3 = m_arena.makeUniformRandom(negOne, posOne);
                const int sum3 = m_arena.makeAdd(m_arena.makeAdd(r1, r2), r3);
                const int scaled = m_arena.makeMul(m_arena.makeMul(halfWidthId, m_arena.makeConst(0.333333)), sum3);
                int val = m_arena.makeAdd(meanId, scaled);
                if (proxy.hasParam(QStringLiteral("minval")) || proxy.hasParam(QStringLiteral("maxval"))) {
                    const int minId = resolveScalarNode(proxy.param(QStringLiteral("minval")), -1e6, t);
                    const int maxId = resolveScalarNode(proxy.param(QStringLiteral("maxval")), 1e6, t);
                    if (t) {
                        taintTarget(resultVar);
                        continue;
                    }
                    val = m_arena.makeClamp(val, minId, maxId);
                }
                writeTarget(resultVar, makeScalar(val, false));
                continue;
            }
            if (name == QLatin1String("entityrandom")) {
                bool t = false;
                const int scaleId = resolveScalarNode(proxy.param(QStringLiteral("scale")), 1.0, t);
                const QString resultVar = proxy.param(QStringLiteral("resultvar"));
                if (t) {
                    taintTarget(resultVar);
                    continue;
                }
                const double seed01 = deterministicSeed01(m_vmt.relativePath);
                const int fallbackId = m_arena.makeMul(m_arena.makeConst(seed01), scaleId);
                const int nodeId = m_arena.makeRenderAttr(QStringLiteral("EntityRandom"), scaleId, fallbackId);
                writeTarget(resultVar, makeScalar(nodeId, false));
                continue;
            }
            if (name == QLatin1String("waterlod")) {
                // Lossless ignore: csgo_water_fancy.vfx has built-in modern LOD
                continue;
            }
            if (name == QLatin1String("breakablesurface")) {
                m_hasBreakableSurface = true;
                continue;
            }

            // ----------------------------------------------------------------
            // Tier C: Entity-driven frame switchers
            // ----------------------------------------------------------------
            if (name == QLatin1String("toggletexture")
                || name == QLatin1String("materialmodify")
                || name == QLatin1String("materialmodifyanimated")
                || name == QLatin1String("counterproxy")) {
                m_hasEntityFrameSwitcher = true;
                if (name == QLatin1String("counterproxy")) {
                    const QString digitStr = proxy.hasParam(QStringLiteral("digit"))
                        ? proxy.param(QStringLiteral("digit"))
                        : proxy.param(QStringLiteral("digitindex"), QStringLiteral("0"));
                    bool ok = false;
                    const int digit = digitStr.trimmed().toInt(&ok);
                    const int safeDigit = (ok && digit >= 0) ? digit : 0;
                    m_entityFrameExpr = QStringLiteral("exists( $DisplayDigit%1 ) ? $DisplayDigit%1 : 0.0").arg(safeDigit);
                } else {
                    m_entityFrameExpr = QStringLiteral("exists( $frame ) ? $frame : 0.0");
                }
                addWarning(QCoreApplication::translate(
                    "MaterialProxyCompiler",
                    "Entity-driven frame proxy '%1' mapped to scripted F_TEXTURE_ANIMATION_MODE 2 with fallback 0.0")
                    .arg(name));
                continue;
            }

            // ----------------------------------------------------------------
            // Tier C: Unsupported / stateful / gameplay / econ / postprocess proxies -> Taint
            // ----------------------------------------------------------------
            const QString outVar = proxy.hasParam(QStringLiteral("resultvar"))
                ? proxy.param(QStringLiteral("resultvar"))
                : (proxy.hasParam(QStringLiteral("texturescrollvar"))
                    ? proxy.param(QStringLiteral("texturescrollvar"))
                    : proxy.param(QStringLiteral("animatedtextureframenumvar")));
            taintTarget(outVar);
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Unsupported runtime/gameplay proxy '%1' skipped; keeping static initial value")
                .arg(name));
        }
    }

    void addDynamicParam(const QString& paramName, const QString& expr)
    {
        if (!isDynamicParamAllowed(m_targetShader, paramName)) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Dynamic parameter '%1' is not supported by target shader and was omitted")
                .arg(paramName));
            return;
        }
        upsertPair(m_plan.dynamicParams, paramName, expr);
        m_plan.hasAnimation = true;
    }

    void addStaticParam(const QString& paramName, const QString& value)
    {
        if (!isStaticParamAllowed(m_targetShader, paramName)) {
            return;
        }
        upsertPair(m_plan.staticParamOverrides, paramName, value);
    }

    void lowerTransform(
        const QString& vmtTransformKey,
        const QString& centerParam,
        const QString& scaleParam,
        const QString& rotateParam,
        const QString& offsetParam,
        const QString& nativeScrollParam)
    {
        const auto it = m_transforms.constFind(vmtTransformKey);
        if (it == m_transforms.constEnd() || !it->modifiedByProxy) {
            return;
        }
        const TransformState& ts = it.value();
        if (ts.tainted) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Transform '%1' was tainted by an unsupported proxy; keeping static initial value")
                .arg(vmtTransformKey));
            return;
        }

        ExprEmitter emitter(m_arena);

        // Scale
        if (ts.scaleModified && !scaleParam.isEmpty()) {
            const bool isDyn = m_arena.node(ts.scale[0]).isDynamic || m_arena.node(ts.scale[1]).isDynamic;
            if (!isDyn) {
                addStaticParam(scaleParam, formatStaticVec2(m_arena.constVal(ts.scale[0]), m_arena.constVal(ts.scale[1])));
            } else {
                addDynamicParam(scaleParam, emitter.emitVec2(ts.scale[0], ts.scale[1]));
            }
        }

        // Center
        if (ts.centerModified && !centerParam.isEmpty()) {
            const bool isDyn = m_arena.node(ts.center[0]).isDynamic || m_arena.node(ts.center[1]).isDynamic;
            if (!isDyn) {
                addStaticParam(centerParam, formatStaticVec2(m_arena.constVal(ts.center[0]), m_arena.constVal(ts.center[1])));
            } else {
                addDynamicParam(centerParam, emitter.emitVec2(ts.center[0], ts.center[1]));
            }
        }

        // Rotate
        if (ts.rotateModified && !rotateParam.isEmpty()) {
            if (!m_arena.node(ts.rotate).isDynamic) {
                addStaticParam(rotateParam, formatStaticFloat(m_arena.constVal(ts.rotate)));
            } else {
                addDynamicParam(rotateParam, emitter.emitScalar(ts.rotate));
            }
        }

        // Translate -> Priority 1: Static fold; Priority 2: GPU native scroll; Priority 3: DynamicParams
        if (ts.translateModified) {
            const int tx = ts.translate[0];
            const int ty = ts.translate[1];
            const bool isDyn = m_arena.node(tx).isDynamic || m_arena.node(ty).isDynamic;

            if (!isDyn) {
                if (!offsetParam.isEmpty()) {
                    addStaticParam(offsetParam, formatStaticVec2(m_arena.constVal(tx), m_arena.constVal(ty)));
                }
                return;
            }

            // Check GPU native scroll eligibility (explicit TextureScroll OR pure linear velocity from LinearRamp)
            bool canUseNativeScroll = false;
            double speedU = 0.0;
            double speedV = 0.0;
            if (ts.isPureConstantScroll) {
                canUseNativeScroll = true;
                speedU = ts.scrollSpeedU;
                speedV = ts.scrollSpeedV;
            } else if (!ts.rotateModified || m_arena.isZero(ts.rotate)) {
                const auto vu = m_arena.matchPureLinearTimeVelocity(tx);
                const auto vv = m_arena.matchPureLinearTimeVelocity(ty);
                if (vu.has_value() && vv.has_value()) {
                    canUseNativeScroll = true;
                    speedU = *vu;
                    speedV = *vv;
                }
            }

            if (canUseNativeScroll && !nativeScrollParam.isEmpty()
                && supportsNativeScrollSlot(m_targetShader, nativeScrollParam)) {
                upsertPair(m_plan.staticParamOverrides, nativeScrollParam, formatStaticVec2(speedU, speedV));
                m_plan.hasAnimation = true;
                if (nativeScrollParam == QLatin1String("g_vSelfIllumScrollSpeed")
                    && m_targetShader == TargetShader::CsgoComplex) {
                    upsertPair(m_plan.featureOverrides, QStringLiteral("F_SELF_ILLUM"), QStringLiteral("1"));
                }
                return;
            }

            // Fallback to Priority 3: DynamicParams UV offset
            if (!offsetParam.isEmpty()) {
                if (canUseNativeScroll) {
                    // Clean idiomatic frac( float2( u_s, v_s ) * time() )
                    const QString expr = QStringLiteral("frac( float2( %1, %2 ) * time() )")
                        .arg(formatExprFloat(speedU), formatExprFloat(speedV));
                    addDynamicParam(offsetParam, expr);
                } else {
                    // Hard guard (§19.3): wrap unbounded LinearRamp UV offsets in frac(...)
                    const int safeTx = m_arena.node(tx).containsLinearRamp ? m_arena.makeFrac(tx) : tx;
                    const int safeTy = m_arena.node(ty).containsLinearRamp ? m_arena.makeFrac(ty) : ty;
                    addDynamicParam(offsetParam, emitter.emitVec2(safeTx, safeTy));
                }
            }
        }
    }

    void lowerColorVar(
        const QString& vmtVar,
        const QString& targetParam,
        double staticAlphaW,
        bool enableSelfIllumFeature)
    {
        const auto it = m_env.constFind(vmtVar);
        if (it == m_env.constEnd() || !it->modifiedByProxy) {
            return;
        }
        const SymValue& sv = it.value();
        if (sv.anyTainted()) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Color parameter '%1' was tainted by an unsupported proxy; keeping static initial value")
                .arg(vmtVar));
            return;
        }

        const int r = sv.comp[0];
        const int g = sv.width >= 2 ? sv.comp[1] : sv.comp[0];
        const int b = sv.width >= 3 ? sv.comp[2] : sv.comp[0];
        const bool isDyn = m_arena.node(r).isDynamic || m_arena.node(g).isDynamic || m_arena.node(b).isDynamic;

        if (!isDyn) {
            if (isStaticParamAllowed(m_targetShader, targetParam)) {
                addStaticParam(targetParam,
                    formatStaticVec4(m_arena.constVal(r), m_arena.constVal(g), m_arena.constVal(b), staticAlphaW));
                if (enableSelfIllumFeature && m_targetShader == TargetShader::CsgoComplex) {
                    upsertPair(m_plan.featureOverrides, QStringLiteral("F_SELF_ILLUM"), QStringLiteral("1"));
                }
            }
            return;
        }

        if (isDynamicParamAllowed(m_targetShader, targetParam)) {
            ExprEmitter emitter(m_arena);
            // Hard guard (§19.3): output sRGB [0..1], NEVER wrap in SrgbGammaToLinear()
            addDynamicParam(targetParam, emitter.emitVec3(r, g, b));
            if (enableSelfIllumFeature && m_targetShader == TargetShader::CsgoComplex) {
                upsertPair(m_plan.featureOverrides, QStringLiteral("F_SELF_ILLUM"), QStringLiteral("1"));
            }
        }
    }

    void lowerScalarVar(
        const QString& vmtVar,
        const QString& targetParam,
        const QString& prerequisiteFeature = {})
    {
        const auto it = m_env.constFind(vmtVar);
        if (it == m_env.constEnd() || !it->modifiedByProxy) {
            return;
        }
        const SymValue& sv = it.value();
        if (sv.tainted[0]) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Scalar parameter '%1' was tainted by an unsupported proxy; keeping static initial value")
                .arg(vmtVar));
            return;
        }

        const int id = sv.comp[0];
        if (!m_arena.node(id).isDynamic) {
            if (isStaticParamAllowed(m_targetShader, targetParam)) {
                const double val = m_arena.constVal(id);
                addStaticParam(targetParam, formatStaticFloat(val));
                if (!prerequisiteFeature.isEmpty() && m_targetShader == TargetShader::CsgoComplex) {
                    if (prerequisiteFeature != QLatin1String("F_TRANSLUCENT") || val < 0.999) {
                        upsertPair(m_plan.featureOverrides, prerequisiteFeature, QStringLiteral("1"));
                    }
                }
            }
            return;
        }

        if (isDynamicParamAllowed(m_targetShader, targetParam)) {
            ExprEmitter emitter(m_arena);
            addDynamicParam(targetParam, emitter.emitScalar(id));
            if (!prerequisiteFeature.isEmpty() && m_targetShader == TargetShader::CsgoComplex) {
                upsertPair(m_plan.featureOverrides, prerequisiteFeature, QStringLiteral("1"));
            }
        }
    }

    void lowerFrameAnimation()
    {
        const QString frameVar = m_hasAnimatedTexture ? m_animatedFrameVar : QStringLiteral("$frame");
        const auto frameIt = m_env.constFind(frameVar);
        const bool frameModifiedByProxy = (frameIt != m_env.constEnd() && frameIt->modifiedByProxy);

        const bool hasAnyFrameAnim = m_hasAnimatedTexture || m_hasEntityFrameSwitcher || frameModifiedByProxy;
        if (!hasAnyFrameAnim) {
            return;
        }

        if (!supportsTextureAnimation(m_targetShader)) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Frame animation is not supported by target shader; skipping F_TEXTURE_ANIMATION"));
            return;
        }

        const int cols = (m_atlasInfo.has_value() && m_atlasInfo->cols > 0) ? m_atlasInfo->cols : 1;
        const int rows = (m_atlasInfo.has_value() && m_atlasInfo->rows > 0) ? m_atlasInfo->rows : 1;
        const int cellCount = (m_atlasInfo.has_value() && m_atlasInfo->cellCount > 0) ? m_atlasInfo->cellCount : 1;

        auto emitAtlasBase = [&]() {
            m_plan.usesTextureAnimation = true;
            m_plan.hasAnimation = true;
            upsertPair(m_plan.featureOverrides, QStringLiteral("F_TEXTURE_ANIMATION"), QStringLiteral("1"));
            upsertPair(m_plan.staticParamOverrides, QStringLiteral("g_vAnimationGrid"),
                QStringLiteral("[%1 %2]").arg(cols).arg(rows));
            upsertPair(m_plan.staticParamOverrides, QStringLiteral("g_nNumAnimationCells"),
                QString::number(cellCount));
        };

        // Case 1: Entity-driven frame switcher (ToggleTexture, MaterialModify*, CounterProxy)
        if (m_hasEntityFrameSwitcher) {
            emitAtlasBase();
            m_plan.animationMode = TextureAnimationMode::Scripted;
            upsertPair(m_plan.featureOverrides, QStringLiteral("F_TEXTURE_ANIMATION_MODE"), QStringLiteral("2"));
            addDynamicParam(QStringLiteral("g_flAnimationFrame"), m_entityFrameExpr);
            return;
        }

        // Check taint on $frame
        if (frameIt != m_env.constEnd() && frameIt->tainted[0]) {
            addWarning(QCoreApplication::translate(
                "MaterialProxyCompiler",
                "Frame parameter '%1' was tainted by an unsupported proxy; keeping static frame 0")
                .arg(frameVar));
            return;
        }

        // Case 2: Pure AnimatedTexture (unmodified downstream, constant positive FPS) -> Mode 0 Sequential
        if (m_hasAnimatedTexture && m_animatedFpsIsConst && frameIt != m_env.constEnd()
            && frameIt->comp[0] == m_animatedFrameInitialNodeId) {
            emitAtlasBase();
            m_plan.animationMode = TextureAnimationMode::Sequential;
            m_plan.animationTimePerFrame = 1.0f / m_animatedFps;
            m_plan.animationTimeOffset = 0.0f;
            upsertPair(m_plan.featureOverrides, QStringLiteral("F_TEXTURE_ANIMATION_MODE"), QStringLiteral("0"));
            upsertPair(m_plan.staticParamOverrides, QStringLiteral("g_flAnimationTimePerFrame"),
                formatStaticFloat(m_plan.animationTimePerFrame));
            upsertPair(m_plan.staticParamOverrides, QStringLiteral("g_flAnimationTimeOffset"),
                formatStaticFloat(m_plan.animationTimeOffset));
            return;
        }

        // Case 3: Scripted $frame expression -> Mode 2 Scripted + DynamicParams.g_flAnimationFrame
        if (frameIt != m_env.constEnd()) {
            emitAtlasBase();
            m_plan.animationMode = TextureAnimationMode::Scripted;
            upsertPair(m_plan.featureOverrides, QStringLiteral("F_TEXTURE_ANIMATION_MODE"), QStringLiteral("2"));

            const int frameNode = frameIt->comp[0];
            if (!m_arena.node(frameNode).isDynamic) {
                addStaticParam(QStringLiteral("g_flAnimationFrame"), formatStaticFloat(m_arena.constVal(frameNode)));
            } else {
                ExprEmitter emitter(m_arena);
                addDynamicParam(QStringLiteral("g_flAnimationFrame"), emitter.emitScalar(frameNode));
            }
        }
    }

    void lowerToPlan()
    {
        // BreakableSurface (csgo_glass.vfx)
        if (m_hasBreakableSurface && m_targetShader == TargetShader::CsgoGlass) {
            upsertPair(m_plan.featureOverrides, QStringLiteral("F_VERTEX_BREAK_ANIMATION"), QStringLiteral("1"));
            addDynamicParam(QStringLiteral("g_flBreakTime"), QStringLiteral("return $ent_age;"));
            addDynamicParam(QStringLiteral("g_flBreakBounceFloor"), QStringLiteral("return $ent_renderbounds.z / 2.0 + 0.25;"));
            addDynamicParam(QStringLiteral("g_vBreakShotPosition"), QStringLiteral("return DamagePositionObjectSpace;"));
        }

        // Transforms per TargetShader
        switch (m_targetShader) {
        case TargetShader::CsgoComplex:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                QStringLiteral("g_vTexCoordCenter"),
                QStringLiteral("g_vTexCoordScale"),
                QStringLiteral("g_flTexCoordRotation"),
                QStringLiteral("g_vTexCoordOffset"),
                QStringLiteral("g_vTexCoordScrollSpeed"));
            lowerTransform(
                QStringLiteral("$detailtexturetransform"),
                QStringLiteral("g_vDetailTexCoordCenter"),
                QStringLiteral("g_vDetailTexCoordScale"),
                QStringLiteral("g_flDetailTexCoordRotation"),
                QStringLiteral("g_vDetailTexCoordOffset"),
                {});
            lowerTransform(
                QStringLiteral("$selfillumtexturetransform"),
                {}, {}, {}, {},
                QStringLiteral("g_vSelfIllumScrollSpeed"));
            break;
        case TargetShader::CsgoEnvironment:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                QStringLiteral("g_vTexCoordCenter1"),
                QStringLiteral("g_vTexCoordScale1"),
                QStringLiteral("g_flTexCoordRotation1"),
                QStringLiteral("g_vTexCoordOffset1"),
                {});
            lowerTransform(
                QStringLiteral("$detailtexturetransform"),
                QStringLiteral("g_vDetailTexCoordCenter1"),
                QStringLiteral("g_vDetailTexCoordScale1"),
                QStringLiteral("g_flDetailTexCoordRotation1"),
                QStringLiteral("g_vDetailTexCoordOffset1"),
                {});
            break;
        case TargetShader::CsgoEnvironmentBlend:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                {},
                QStringLiteral("g_vTexCoordScale1"),
                QStringLiteral("g_flTexCoordRotation1"),
                QStringLiteral("g_vTexCoordOffset1"),
                {});
            lowerTransform(
                QStringLiteral("$basetexturetransform2"),
                {},
                QStringLiteral("g_vTexCoordScale2"),
                QStringLiteral("g_flTexCoordRotation2"),
                QStringLiteral("g_vTexCoordOffset2"),
                {});
            break;
        case TargetShader::CsgoStaticOverlay:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                QStringLiteral("g_vTexCoordCenter"),
                QStringLiteral("g_vTexCoordScale"),
                QStringLiteral("g_flTexCoordRotation"),
                QStringLiteral("g_vTexCoordOffset"),
                QStringLiteral("g_vTexCoordScrollSpeed"));
            break;
        case TargetShader::CsgoEffects:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                {}, {}, {}, {},
                QStringLiteral("g_vTexCoordScrollSpeed"));
            lowerTransform(
                QStringLiteral("$texture2transform"),
                {},
                QStringLiteral("g_vMask1Scale"),
                {}, {},
                QStringLiteral("g_vMask1PanSpeed"));
            break;
        case TargetShader::CsgoWaterFancy:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                QStringLiteral("g_vTexCoordCenter"),
                QStringLiteral("g_vTexCoordScale"),
                QStringLiteral("g_flTexCoordRotation"),
                QStringLiteral("g_vTexCoordOffset"),
                QStringLiteral("g_vTexCoordScrollSpeed"));
            break;
        case TargetShader::CsgoRefract:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                {},
                QStringLiteral("g_vTexCoordScale"),
                QStringLiteral("g_flTexCoordRotation"),
                QStringLiteral("g_vTexCoordOffset"),
                {});
            break;
        case TargetShader::CsgoFoliage:
            lowerTransform(
                QStringLiteral("$basetexturetransform"),
                {}, {}, {}, {},
                QStringLiteral("g_vTexCoordScrollSpeed"));
            break;
        default:
            break;
        }

        // Color / Tint / Alpha / SelfIllum / Detail / Refract / Boost
        const double colorAlphaW = (m_targetShader == TargetShader::CsgoComplex) ? 1.0 : 0.0;
        lowerColorVar(QStringLiteral("$color"), QStringLiteral("g_vColorTint"), colorAlphaW, false);
        lowerColorVar(QStringLiteral("$selfillumtint"), QStringLiteral("g_vSelfIllumTint"), 0.0, true);
        lowerScalarVar(QStringLiteral("$alpha"), QStringLiteral("g_flOpacityScale"), QStringLiteral("F_TRANSLUCENT"));
        lowerScalarVar(QStringLiteral("$selfillumbrightness"), QStringLiteral("g_flSelfIllumBrightness"), QStringLiteral("F_SELF_ILLUM"));
        lowerScalarVar(QStringLiteral("$selfillumscale"), QStringLiteral("g_flSelfIllumScale"), QStringLiteral("F_SELF_ILLUM"));
        lowerScalarVar(QStringLiteral("$detailblendfactor"), QStringLiteral("g_flDetailBlendFactor"));
        lowerScalarVar(QStringLiteral("$refractamount"), QStringLiteral("g_flRefractScale"));
        lowerScalarVar(QStringLiteral("$overbrightfactor"), QStringLiteral("g_flColorBoost"));

        // Frame animation
        lowerFrameAnimation();
    }

    const VmtMaterial& m_vmt;
    TargetShader m_targetShader;
    std::optional<TextureAtlasFrameInfo> m_atlasInfo;

    ExprArena m_arena;
    QHash<QString, SymValue> m_env;
    QHash<QString, TransformState> m_transforms;

    bool m_hasAnimatedTexture = false;
    QString m_animatedFrameVar = QStringLiteral("$frame");
    float m_animatedFps = 10.0f;
    bool m_animatedFpsIsConst = false;
    int m_animatedFrameInitialNodeId = -1;

    bool m_hasEntityFrameSwitcher = false;
    QString m_entityFrameExpr;
    bool m_hasBreakableSurface = false;

    MaterialAnimationPlan m_plan;
};

} // namespace

// ============================================================================
// MaterialProxyCompiler Public API
// ============================================================================

ProxyRoutingHints MaterialProxyCompiler::analyzeRoutingHints(const VmtMaterial& vmt)
{
    ProxyRoutingHints hints;
    if (vmt.proxies.isEmpty()) {
        return hints;
    }

    bool hasBaseTransformProxy = false;
    bool hasTex2TransformProxy = false;

    for (const auto& proxy : vmt.proxies) {
        const QString& name = proxy.name;

        if (name == QLatin1String("breakablesurface")) {
            hints.hasBreakableSurface = true;
            continue;
        }
        if (name == QLatin1String("waterlod")) {
            continue;
        }

        if (name == QLatin1String("animatedtexture")
            || name == QLatin1String("animatedentitytexture")
            || name == QLatin1String("animatedoffsettexture")
            || name == QLatin1String("toggletexture")
            || name == QLatin1String("materialmodify")
            || name == QLatin1String("materialmodifyanimated")
            || name == QLatin1String("counterproxy")) {
            hints.hasFrameAnimation = true;
        }

        if (name == QLatin1String("texturescroll")) {
            const QString target = proxy.param(QStringLiteral("texturescrollvar"), QStringLiteral("$basetexturetransform"))
                                       .trimmed().toLower();
            if (target == QLatin1String("$basetexturetransform")) {
                hasBaseTransformProxy = true;
            } else if (target == QLatin1String("$texture2transform")) {
                hasTex2TransformProxy = true;
            }
        } else if (name == QLatin1String("texturetransform")) {
            const QString target = proxy.param(QStringLiteral("resultvar"), QStringLiteral("$basetexturetransform"))
                                       .trimmed().toLower();
            if (target == QLatin1String("$basetexturetransform")) {
                hasBaseTransformProxy = true;
            } else if (target == QLatin1String("$texture2transform")) {
                hasTex2TransformProxy = true;
            }
        }

        const QString resultVar = proxy.param(QStringLiteral("resultvar")).trimmed().toLower();
        if (!resultVar.isEmpty()) {
            const auto ref = parseVarRef(resultVar);
            if (ref.has_value()) {
                if (ref->baseName == QLatin1String("$frame")) {
                    hints.hasFrameAnimation = true;
                } else if (ref->baseName == QLatin1String("$alpha")) {
                    hints.hasDynamicAlpha = true;
                }
            }
        }
    }

    if (hasTex2TransformProxy && (hasBaseTransformProxy || vmt.hasParam(QStringLiteral("$texture2")))) {
        hints.hasDualAsyncScroll = true;
    }

    return hints;
}

MaterialAnimationPlan MaterialProxyCompiler::compile(
    const VmtMaterial& vmt,
    TargetShader targetShader,
    const std::optional<TextureAtlasFrameInfo>& atlasInfo)
{
    if (vmt.proxies.isEmpty()) {
        return {};
    }
    ProxyCompileSession session(vmt, targetShader, atlasInfo);
    return session.run();
}

} // namespace Domain::Material
