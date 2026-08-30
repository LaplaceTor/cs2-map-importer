#include "Domain/Audio/SoundEventKv3Writer.h"
#include "Core/FileSystem/AtomicFile.h"
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

namespace Domain::Audio {

namespace {

QString formatDouble(double v) {
    QString s = QString::number(v, 'f', 6);
    while (s.contains('.') && (s.endsWith('0') || s.endsWith('.'))) {
        if (s.endsWith('.')) {
            s.chop(1);
            break;
        }
        s.chop(1);
    }
    return s;
}

QString formatEventKey(const QString& name) {
    if (name.contains(' ') || name.contains('"') || name.contains('\\')) {
        return QStringLiteral("\"%1\"").arg(name);
    }
    return name;
}

} // namespace

void SoundEventKv3Writer::writeCurve(QString& out, const QString& curveName, const std::vector<CurveControlPoint>& points) {
    if (points.empty()) {
        return;
    }

    out += QStringLiteral("\t\t%1 = \n\t\t[\n").arg(curveName);
    for (const auto& pt : points) {
        out += QStringLiteral("\t\t\t[\n");
        out += QStringLiteral("\t\t\t\t%1, %2, %3, %4,\n")
                   .arg(formatDouble(pt.x), formatDouble(pt.y), formatDouble(pt.slopeIn), formatDouble(pt.slopeOut));
        out += QStringLiteral("\t\t\t\t%1, %2,\n")
                   .arg(formatDouble(pt.modeIn), formatDouble(pt.modeOut));
        out += QStringLiteral("\t\t\t],\n");
    }
    out += QStringLiteral("\t\t]\n");
}

void SoundEventKv3Writer::writeSoundEvent(QString& out, const SoundEvent& ev) {
    out += QStringLiteral("\t%1 = \n\t{\n").arg(formatEventKey(ev.name));
    out += QStringLiteral("\t\ttype = \"%1\"\n").arg(ev.type);

    if (!ev.mixgroup.isEmpty()) {
        out += QStringLiteral("\t\tmixgroup = \"%1\"\n").arg(ev.mixgroup);
    }

    out += QStringLiteral("\t\tvolume = %1\n").arg(formatDouble(ev.volume));
    if (ev.volumeRandomMin.has_value()) {
        out += QStringLiteral("\t\tvolume_random_min = %1\n").arg(formatDouble(*ev.volumeRandomMin));
    }
    if (ev.volumeRandomMax.has_value()) {
        out += QStringLiteral("\t\tvolume_random_max = %1\n").arg(formatDouble(*ev.volumeRandomMax));
    }

    out += QStringLiteral("\t\tpitch = %1\n").arg(formatDouble(ev.pitch));
    if (ev.pitchRandomMin.has_value()) {
        out += QStringLiteral("\t\tpitch_random_min = %1\n").arg(formatDouble(*ev.pitchRandomMin));
    }
    if (ev.pitchRandomMax.has_value()) {
        out += QStringLiteral("\t\tpitch_random_max = %1\n").arg(formatDouble(*ev.pitchRandomMax));
    }

    if (ev.enableRetrigger) {
        out += QStringLiteral("\t\tenable_retrigger = true\n");
        if (ev.retriggerIntervalMin.has_value()) {
            out += QStringLiteral("\t\tretrigger_interval_min = %1\n").arg(formatDouble(*ev.retriggerIntervalMin));
        }
        if (ev.retriggerIntervalMax.has_value()) {
            out += QStringLiteral("\t\tretrigger_interval_max = %1\n").arg(formatDouble(*ev.retriggerIntervalMax));
        }
    }

    if (ev.position.has_value()) {
        out += QStringLiteral("\t\tposition = [ %1, %2, %3 ]\n")
                   .arg(formatDouble(ev.position->x), formatDouble(ev.position->y), formatDouble(ev.position->z));
    }

    if (ev.useWorldPosition) {
        out += QStringLiteral("\t\tuse_world_position = true\n");
    }
    if (ev.positionRelativeToPlayer) {
        out += QStringLiteral("\t\tposition_relative_to_player = true\n");
    }
    if (ev.randomizePositionMinRadius.has_value()) {
        out += QStringLiteral("\t\trandomize_position_min_radius = %1\n").arg(formatDouble(*ev.randomizePositionMinRadius));
    }
    if (ev.randomizePositionMaxRadius.has_value()) {
        out += QStringLiteral("\t\trandomize_position_max_radius = %1\n").arg(formatDouble(*ev.randomizePositionMaxRadius));
    }
    if (ev.randomizePositionHemisphere.has_value()) {
        out += QStringLiteral("\t\trandomize_position_hemisphere = %1\n").arg(*ev.randomizePositionHemisphere ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (ev.enableChildEvents) {
        out += QStringLiteral("\t\tenable_child_events = true\n");
        out += QStringLiteral("\t\tset_child_position = %1\n").arg(ev.setChildPosition ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (!ev.dspPreset.isEmpty()) {
        out += QStringLiteral("\t\tdsp_preset = \"%1\"\n").arg(ev.dspPreset);
    }
    if (ev.overrideDspPreset) {
        out += QStringLiteral("\t\toverride_dsp_preset = true\n");
    }
    if (ev.reverbWet.has_value()) {
        out += QStringLiteral("\t\treverb_wet = %1\n").arg(formatDouble(*ev.reverbWet));
    }
    if (ev.restrictSourceReverb) {
        out += QStringLiteral("\t\trestrict_source_reverb = true\n");
    }
    out += QStringLiteral("\t\tdistance_effect_mix = %1\n").arg(formatDouble(ev.distanceEffectMix));

    if (ev.useDistanceVolumeMappingCurve) {
        out += QStringLiteral("\t\tuse_distance_volume_mapping_curve = true\n");
        writeCurve(out, QStringLiteral("distance_volume_mapping_curve"), ev.distanceVolumeMappingCurve);
    }

    if (ev.useTimeVolumeMappingCurve) {
        out += QStringLiteral("\t\tuse_time_volume_mapping_curve = true\n");
        writeCurve(out, QStringLiteral("time_volume_mapping_curve"), ev.timeVolumeMappingCurve);
        writeCurve(out, QStringLiteral("fadetime_volume_mapping_curve"), ev.fadetimeVolumeMappingCurve);
    }

    if (!ev.childEvents.isEmpty()) {
        out += QStringLiteral("\t\tsoundevent_01 = \n\t\t[\n");
        for (const auto& child : ev.childEvents) {
            out += QStringLiteral("\t\t\t\"%1\",\n").arg(child);
        }
        out += QStringLiteral("\t\t]\n");
    }

    if (ev.vsndFiles.size() == 1) {
        out += QStringLiteral("\t\tvsnd_files_track_01 = \"%1\"\n").arg(ev.vsndFiles.first());
    } else if (ev.vsndFiles.size() > 1) {
        out += QStringLiteral("\t\tvsnd_files_track_01 = \n\t\t[\n");
        for (const auto& file : ev.vsndFiles) {
            out += QStringLiteral("\t\t\t\"%1\",\n").arg(file);
        }
        out += QStringLiteral("\t\t]\n");
    }

    out += QStringLiteral("\t}\n");
}

QString SoundEventKv3Writer::writeToString(const std::vector<SoundEvent>& events) {
    QString out;
    out.reserve(events.size() * 512 + 256);

    out += QStringLiteral("<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n{\n");

    for (const auto& ev : events) {
        writeSoundEvent(out, ev);
    }

    out += QStringLiteral("}\n");
    return out;
}

Core::Result<void> SoundEventKv3Writer::writeToFile(const Core::Path::FilesystemPath& filePath, const std::vector<SoundEvent>& events) {
    try {
        const QString content = writeToString(events);
        const QByteArray data = content.toUtf8();
        Core::FileSystem::AtomicFile::writeAtomic(filePath.toString(), data);
        return Core::Result<void>::success();
    } catch (const Core::Error::Exception& ex) {
        return Core::Result<void>::failure(ex.error(), QStringLiteral("Failed to write KV3 soundevents file"));
    } catch (const std::exception& ex) {
        return Core::Result<void>::failure(Core::Error::ErrorCode::WriteFailed, QString::fromUtf8(ex.what()), filePath.toString());
    } catch (...) {
        return Core::Result<void>::failure(Core::Error::ErrorCode::Unknown, QStringLiteral("Unknown error writing KV3 soundevents file"), filePath.toString());
    }
}

} // namespace Domain::Audio

