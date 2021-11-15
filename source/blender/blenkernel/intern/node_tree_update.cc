/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_map.hh"

#include "DNA_node_types.h"

#include "BKE_node.h"
#include "BKE_node_tree_update.h"

namespace blender::bke {

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  NodeTreeUpdateExtraParams *params_;

 public:
  NodeTreeMainUpdater(Main *bmain, NodeTreeUpdateExtraParams *params)
      : bmain_(bmain), params_(params)
  {
  }

  void update()
  {
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      ntreeUpdateTree(bmain_, ntree);
      if (params_) {
        if (params_->tree_changed_fn) {
          params_->tree_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_interface_changed_fn) {
          params_->tree_interface_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_output_changed_fn) {
          params_->tree_output_changed_fn(id, ntree, params_->user_data);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  void update_rooted(Span<bNodeTree *> root_trees)
  {
    this->update();
  }
};

}  // namespace blender::bke

void BKE_node_tree_update_tag(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node(bNodeTree *ntree, bNode *node)
{
  ntree->changed_flag |= NTREE_CHANGED_NODE;
  node->changed_flag |= NODE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_socket(bNodeTree *ntree, bNodeSocket *socket)
{
  ntree->changed_flag |= NTREE_CHANGED_SOCKET;
  socket->changed_flag |= SOCK_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_removed(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_REMOVED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_link(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_LINK;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_node_added(bNodeTree *ntree, bNode *node)
{
  BKE_node_tree_update_tag_node(ntree, node);
}

void BKE_node_tree_update_tag_link_removed(bNodeTree *ntree)
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_added(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_link_mute(bNodeTree *ntree, bNodeLink *UNUSED(link))
{
  BKE_node_tree_update_tag_link(ntree);
}

void BKE_node_tree_update_tag_missing_runtime_data(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_MISSING_RUNTIME_DATA;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_tag_interface(bNodeTree *ntree)
{
  ntree->changed_flag |= NTREE_CHANGED_ANY;
  ntree->update |= NTREE_UPDATE;
}

void BKE_node_tree_update_main(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update();
}

void BKE_node_tree_update_main_rooted(Main *bmain,
                                      bNodeTree *ntree,
                                      NodeTreeUpdateExtraParams *params)
{
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update_rooted({ntree});
}