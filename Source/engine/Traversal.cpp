#include "engine/Traversal.h"

#include <queue>

namespace chimedes::engine
{

std::vector<model::NodeId> visitOrder (const Tree& tree, model::Traversal traversal)
{
    std::vector<model::NodeId> order;

    if (tree.root == model::noNode)
        return order;

    if (traversal == model::Traversal::BFS)
    {
        std::queue<model::NodeId> pending;
        pending.push (tree.root);

        while (! pending.empty())
        {
            const auto current = pending.front();
            pending.pop();
            order.push_back (current);

            for (const auto child : tree.childrenOf (current))
                pending.push (child);
        }
    }
    else
    {
        // Preorder: children are pushed in reverse so the closest one is visited first.
        std::vector<model::NodeId> stack { tree.root };

        while (! stack.empty())
        {
            const auto current = stack.back();
            stack.pop_back();
            order.push_back (current);

            const auto& kids = tree.childrenOf (current);
            for (auto it = kids.rbegin(); it != kids.rend(); ++it)
                stack.push_back (*it);
        }
    }

    return order;
}

} // namespace chimedes::engine
