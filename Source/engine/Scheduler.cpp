#include "engine/Scheduler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include "engine/Traversal.h"
#include "engine/TreeBuilder.h"
#include "model/Instruments.h"
#include "model/Units.h"

namespace chimedes::engine
{

using model::NodeId;

namespace
{
    NoteEvent makeEvent (const model::Track& track, const model::Node& node, double startBeat,
                         NodeId from = model::noNode)
    {
        return { startBeat, node.durationBeats, node.pitches, node.velocity, node.id,
                 model::resolveInstrument (track, node), from };
    }

    // Timeline mode: there is no tree, so the "source" node is the connected
    // neighbour that sounded most recently before it.
    void linkTimelineSources (const model::Track& track, Schedule& schedule)
    {
        for (auto& event : schedule.events)
        {
            double latest = -1.0;

            for (const auto& edge : track.edges)
            {
                NodeId other = model::noNode;
                if (edge.to == event.node)                        other = edge.from;
                else if (! track.directed && edge.from == event.node) other = edge.to;

                if (other == model::noNode)
                    continue;

                for (const auto& candidate : schedule.events)
                    if (candidate.node == other && candidate.startBeat < event.startBeat
                        && candidate.startBeat > latest)
                    {
                        latest = candidate.startBeat;
                        event.from = other;
                    }
            }
        }
    }

    // Wait in beats for an edge. With beat snapping, it is rounded to the nearest
    // subdivision, never below one subdivision so the order is kept.
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

    // Timeline mode: each node sounds at beat x / 100. The cycle goes from the X of
    // the start node until the end node finishes sounding; anything outside is silent.
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
                schedule.events.push_back (makeEvent (track, node, model::pixelsToBeats (node.x - start.x)));

        schedule.cycleLengthBeats = model::pixelsToBeats (end.x - start.x) + end.durationBeats;
        sortEvents (schedule);
        linkTimelineSources (track, schedule);

        result.schedule = std::move (schedule);
        return result;
    }

    // Algorithm mode, siblings in sequence: each node starts after the previously
    // visited one, waiting the length of the edge that joins it to its parent.
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
            const auto from = id != tree.root ? tree.parent.at (id) : model::noNode;
            schedule.events.push_back (makeEvent (track, node, time, from));

            if (id == track.endNode)
            {
                schedule.cycleLengthBeats = time + node.durationBeats;
                break;
            }
        }

        return schedule;
    }

    // Algorithm mode, simultaneous siblings: visiting a node also plays every sibling
    // not played yet. The group waits the length of its shortest edge.
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
                const auto from = member != tree.root ? tree.parent.at (member) : model::noNode;
                schedule.events.push_back (makeEvent (track, node, time, from));

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
