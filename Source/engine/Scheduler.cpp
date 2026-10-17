#include "engine/Scheduler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include "engine/Traversal.h"
#include "engine/TreeBuilder.h"
#include "model/Units.h"

namespace chimedes::engine
{

using model::NodeId;

namespace
{
    NoteEvent makeEvent (const model::Node& node, double startBeat)
    {
        return { startBeat, node.durationBeats, node.pitches, node.velocity, node.id };
    }

    // Espera en pulsos correspondiente a una arista. Con ajuste al pulso, se redondea a
    // la subdivisión más cercana, sin bajar de una subdivisión para conservar el orden.
    double waitBeats (double pixels, double quantize)
    {
        const auto beats = model::pixelsToBeats (pixels);

        if (quantize <= 0.0)
            return beats;

        return std::max (quantize, std::round (beats / quantize) * quantize);
    }

    void sortEvents (Schedule& schedule)
    {
        std::stable_sort (schedule.events.begin(), schedule.events.end(),
                          [] (const NoteEvent& a, const NoteEvent& b)
                          {
                              return a.startBeat != b.startBeat ? a.startBeat < b.startBeat
                                                                : a.node < b.node;
                          });
    }

    // Modo línea de tiempo: cada nodo suena en el pulso x / 100. El ciclo va de la X
    // del nodo de inicio hasta que acaba de sonar el de fin; lo que queda fuera no suena.
    ScheduleResult scheduleTimeline (const model::Track& track, ScheduleResult result)
    {
        const auto& start = *track.findNode (track.startNode);
        const auto& end = *track.findNode (track.endNode);

        if (end.x < start.x)
        {
            result.error = ScheduleResult::Error::EndBeforeStart;
            return result;
        }

        Schedule schedule;

        for (const auto& node : track.nodes)
            if (node.x >= start.x && node.x <= end.x)
                schedule.events.push_back (makeEvent (node, model::pixelsToBeats (node.x - start.x)));

        schedule.cycleLengthBeats = model::pixelsToBeats (end.x - start.x) + end.durationBeats;
        sortEvents (schedule);

        result.schedule = std::move (schedule);
        return result;
    }

    // Modo algoritmo, hermanos en secuencia: cada nodo empieza después del anterior
    // visitado, esperando la longitud de la arista que lo une a su padre.
    Schedule scheduleSequential (const model::Track& track, const Tree& tree,
                                 const std::vector<NodeId>& order)
    {
        Schedule schedule;
        double time = 0.0;

        for (const auto id : order)
        {
            if (id != tree.root)
                time += waitBeats (tree.parentEdgeLength.at (id), track.quantizeBeats);

            const auto& node = *track.findNode (id);
            schedule.events.push_back (makeEvent (node, time));

            if (id == track.endNode)
            {
                schedule.cycleLengthBeats = time + node.durationBeats;
                break;
            }
        }

        return schedule;
    }

    // Modo algoritmo, hermanos simultáneos: al visitar un nodo suenan con él todos sus
    // hermanos aún no tocados. El grupo espera la longitud de su arista más corta.
    Schedule scheduleSimultaneous (const model::Track& track, const Tree& tree,
                                   const std::vector<NodeId>& order)
    {
        Schedule schedule;
        std::unordered_set<NodeId> played;
        double time = 0.0;
        bool endPlayed = false;

        for (const auto id : order)
        {
            if (played.contains (id))
                continue;

            std::vector<NodeId> group;

            if (id == tree.root)
            {
                group.push_back (id);
            }
            else
            {
                double shortest = std::numeric_limits<double>::max();

                for (const auto sibling : tree.childrenOf (tree.parent.at (id)))
                {
                    if (played.contains (sibling))
                        continue;

                    group.push_back (sibling);
                    shortest = std::min (shortest, tree.parentEdgeLength.at (sibling));
                }

                time += waitBeats (shortest, track.quantizeBeats);
            }

            for (const auto member : group)
            {
                played.insert (member);
                const auto& node = *track.findNode (member);
                schedule.events.push_back (makeEvent (node, time));

                if (member == track.endNode)
                {
                    schedule.cycleLengthBeats = time + node.durationBeats;
                    endPlayed = true;
                }
            }

            if (endPlayed)
                break;
        }

        return schedule;
    }

    ScheduleResult scheduleAlgorithm (const model::Track& track, ScheduleResult result)
    {
        const auto tree = buildTree (track, track.treeMethod);

        if (! tree.has_value())
        {
            result.error = ScheduleResult::Error::TreeUnavailable;
            return result;
        }

        const auto order = visitOrder (*tree, track.traversal);

        auto schedule = track.siblings == model::SiblingMode::Sequential
                            ? scheduleSequential (track, *tree, order)
                            : scheduleSimultaneous (track, *tree, order);

        sortEvents (schedule);
        result.schedule = std::move (schedule);
        return result;
    }
} // namespace

ScheduleResult buildSchedule (const model::Track& track)
{
    ScheduleResult result;
    result.validation = validate (track);

    if (! result.validation.ok())
    {
        result.error = ScheduleResult::Error::Invalid;
        return result;
    }

    return track.mode == model::PlaybackMode::Timeline ? scheduleTimeline (track, std::move (result))
                                                       : scheduleAlgorithm (track, std::move (result));
}

} // namespace chimedes::engine
