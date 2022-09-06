/*
 *  Copyright (c) 2010 Gedare Bloom.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#include <stdbool.h>
#include "base/rb.h"

/** @brief Validate and fix-up tree properties for a new insert/colored node
 *
 *  This routine checks and fixes the Red-Black Tree properties based on
 *  @a the_node being just added to the tree.
 *
 *  @note It does NOT disable interrupts to ensure the atomicity of the
 *        append operation.
 */
static void _RBTree_Validate_insert( RBTree_Node *the_node )
{
  RBTree_Node *parent = _RBTree_Parent( the_node );
  RBTree_Node *grandparent = _RBTree_Parent( parent );

  /* note: the insert root case is handled already */
  /* if the parent is black, nothing needs to be done
   * otherwise may need to loop a few times */
  while ( parent->color == RBT_RED ) {
    /* The root is black, so the grandparent must exist */
    RBTree_Node *uncle = _RBTree_Sibling( parent, grandparent );

    /*
     * If uncle exists and is red, repaint uncle/parent black and grandparent
     * red.
     */
    if ( uncle != NULL && uncle->color == RBT_RED ) {
      parent->color = RBT_BLACK;
      uncle->color = RBT_BLACK;
      grandparent->color = RBT_RED;
      the_node = grandparent;
      parent = _RBTree_Parent( the_node );
      grandparent = _RBTree_Parent( parent );

      if ( grandparent == NULL )
        break;
    } else { /* If uncle does not exist or is black */
      RBTree_Direction dir = _RBTree_Direction( the_node, parent );
      RBTree_Direction parentdir = _RBTree_Direction( parent, grandparent );

      /* ensure node is on the same branch direction as parent */
      if ( dir != parentdir ) {
        RBTree_Node *oldparent = parent;

        parent = the_node;
        the_node = oldparent;
        _RBTree_Rotate( oldparent, parentdir );
      }

      parent->color = RBT_BLACK;
      grandparent->color = RBT_RED;

      /* now rotate grandparent in the other branch direction (toward uncle) */
      _RBTree_Rotate( grandparent, _RBTree_Opposite_direction( parentdir ) );

      grandparent = _RBTree_Parent( parent );
      break;
    }
  }

  if ( grandparent == NULL )
    the_node->color = RBT_BLACK;
}

/** @brief  Validate and fix-up tree properties after deleting a node
 *
 *  This routine is called on a black node, @a the_node, after its deletion.
 *  This function maintains the properties of the red-black tree.
 *
 *  @note It does NOT disable interrupts to ensure the atomicity
 *        of the extract operation.
 */
static void rbtree_extract_validate( RBTree_Node *the_node )
{
  RBTree_Node     *parent;

  parent = the_node->parent;

  if ( !parent->parent )
    return;

  /* continue to correct tree as long as the_node is black and not the root */
  while ( !_RBTree_Is_red( the_node ) && parent->parent ) {
    RBTree_Node *sibling = _RBTree_Sibling( the_node, parent );

    /* if sibling is red, switch parent (black) and sibling colors,
     * then rotate parent left, making the sibling be the_node's grandparent.
     * Now the_node has a black sibling and red parent. After rotation,
     * update sibling pointer.
     */
    if ( _RBTree_Is_red( sibling ) ) {
      RBTree_Direction dir = _RBTree_Direction( the_node, parent );
      RBTree_Direction opp_dir = _RBTree_Opposite_direction( dir );

      parent->color = RBT_RED;
      sibling->color = RBT_BLACK;
      _RBTree_Rotate( parent, dir );
      sibling = parent->child[ opp_dir ];
    }

    /* sibling is black, see if both of its children are also black. */
    if ( !_RBTree_Is_red( sibling->child[ RBT_RIGHT ] ) &&
         !_RBTree_Is_red( sibling->child[ RBT_LEFT ] ) ) {
      sibling->color = RBT_RED;

      if ( _RBTree_Is_red( parent ) ) {
        parent->color = RBT_BLACK;
        break;
      }

      the_node = parent;   /* done if parent is red */
      parent = the_node->parent;
    } else {
      /* at least one of sibling's children is red. we now proceed in two
       * cases, either the_node is to the left or the right of the parent.
       * In both cases, first check if one of sibling's children is black,
       * and if so rotate in the proper direction and update sibling pointer.
       * Then switch the sibling and parent colors, and rotate through parent.
       */
      RBTree_Direction dir = _RBTree_Direction( the_node, parent );
      RBTree_Direction opp_dir = _RBTree_Opposite_direction( dir );

      if (
        !_RBTree_Is_red( sibling->child[ opp_dir ] )
      ) {
        sibling->color = RBT_RED;
        sibling->child[ dir ]->color = RBT_BLACK;
        _RBTree_Rotate( sibling, opp_dir );
        sibling = parent->child[ opp_dir ];
      }

      sibling->color = parent->color;
      parent->color = RBT_BLACK;
      sibling->child[ opp_dir ]->color = RBT_BLACK;
      _RBTree_Rotate( parent, dir );
      break; /* done */
    }
  } /* while */

  if ( !the_node->parent->parent )
    the_node->color = RBT_BLACK;
}

void rbtree_extract(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node
)
{
  RBTree_Node     *leaf, *target;
  RBTree_Color     victim_color;
  RBTree_Direction dir;

  /* check if min needs to be updated */
  if ( the_node == the_rbtree->first[ RBT_LEFT ] ) {
    RBTree_Node *next;
    next = _RBTree_Successor( the_node );
    the_rbtree->first[ RBT_LEFT ] = next;
  }

  /* Check if max needs to be updated. min=max for 1 element trees so
   * do not use else if here. */
  if ( the_node == the_rbtree->first[ RBT_RIGHT ] ) {
    RBTree_Node *previous;
    previous = _RBTree_Predecessor( the_node );
    the_rbtree->first[ RBT_RIGHT ] = previous;
  }

  /* if the_node has at most one non-null child then it is safe to proceed
   * check if both children are non-null, if so then we must find a target node
   * either max in node->child[RBT_LEFT] or min in node->child[RBT_RIGHT],
   * and replace the_node with the target node. This maintains the binary
   * search tree property, but may violate the red-black properties.
   */

  if ( the_node->child[ RBT_LEFT ] && the_node->child[ RBT_RIGHT ] ) {
    target = the_node->child[ RBT_LEFT ]; /* find max in node->child[RBT_LEFT] */

    while ( target->child[ RBT_RIGHT ] )
      target = target->child[ RBT_RIGHT ];

    /* if the target node has a child, need to move it up the tree into
     * target's position (target is the right child of target->parent)
     * when target vacates it. if there is no child, then target->parent
     * should become NULL. This may cause the coloring to be violated.
     * For now we store the color of the node being deleted in victim_color.
     */
    leaf = target->child[ RBT_LEFT ];

    if ( leaf ) {
      leaf->parent = target->parent;
    } else {
      /* fix the tree here if the child is a null leaf. */
      rbtree_extract_validate( target );
    }

    victim_color = target->color;
    dir = target != target->parent->child[ 0 ];
    target->parent->child[ dir ] = leaf;

    /* now replace the_node with target */
    dir = the_node != the_node->parent->child[ 0 ];
    the_node->parent->child[ dir ] = target;

    /* set target's new children to the original node's children */
    target->child[ RBT_RIGHT ] = the_node->child[ RBT_RIGHT ];

    if ( the_node->child[ RBT_RIGHT ] )
      the_node->child[ RBT_RIGHT ]->parent = target;

    target->child[ RBT_LEFT ] = the_node->child[ RBT_LEFT ];

    if ( the_node->child[ RBT_LEFT ] )
      the_node->child[ RBT_LEFT ]->parent = target;

    /* finally, update the parent node and recolor. target has completely
     * replaced the_node, and target's child has moved up the tree if needed.
     * the_node is no longer part of the tree, although it has valid pointers
     * still.
     */
    target->parent = the_node->parent;
    target->color = the_node->color;
  } else {
    /* the_node has at most 1 non-null child. Move the child in to
     * the_node's location in the tree. This may cause the coloring to be
     * violated. We will fix it later.
     * For now we store the color of the node being deleted in victim_color.
     */
    leaf = the_node->child[ RBT_LEFT ] ?
           the_node->child[ RBT_LEFT ] : the_node->child[ RBT_RIGHT ];

    if ( leaf ) {
      leaf->parent = the_node->parent;
    } else {
      /* fix the tree here if the child is a null leaf. */
      rbtree_extract_validate( the_node );
    }

    victim_color = the_node->color;

    /* remove the_node from the tree */
    dir = the_node != the_node->parent->child[ 0 ];
    the_node->parent->child[ dir ] = leaf;
  }

  /* fix coloring. leaf has moved up the tree. The color of the deleted
   * node is in victim_color. There are two cases:
   *   1. Deleted a red node, its child must be black. Nothing must be done.
   *   2. Deleted a black node, its child must be red. Paint child black.
   */
  if ( victim_color == RBT_BLACK ) { /* eliminate case 1 */
    if ( leaf ) {
      leaf->color = RBT_BLACK; /* case 2 */
    }
  }

  /* set root to black, if it exists */
  if ( the_rbtree->root )
    the_rbtree->root->color = RBT_BLACK;
}

RBTree_Node *rbtree_insert(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node,
  RBTree_Compare  compare,
  bool            is_unique
)
{
  RBTree_Node *iter_node = the_rbtree->root;

  if ( !iter_node ) { /* special case: first node inserted */
    the_node->color = RBT_BLACK;
    the_rbtree->root = the_node;
    the_rbtree->first[ 0 ] = the_rbtree->first[ 1 ] = the_node;
    the_node->parent = (RBTree_Node *) the_rbtree;
    the_node->child[ RBT_LEFT ] = the_node->child[ RBT_RIGHT ] = NULL;
  } else {
    /* typical binary search tree insert, descend tree to leaf and insert */
    while ( iter_node ) {
      RBTree_Compare_result compare_result =
        ( *compare )( the_node, iter_node );

      if ( is_unique && _RBTree_Is_equal( compare_result ) )
        return iter_node;

      RBTree_Direction dir = !_RBTree_Is_lesser( compare_result );

      if ( !iter_node->child[ dir ] ) {
        the_node->child[ RBT_LEFT ] = the_node->child[ RBT_RIGHT ] = NULL;
        the_node->color = RBT_RED;
        iter_node->child[ dir ] = the_node;
        the_node->parent = iter_node;
        /* update min/max */
        compare_result = ( *compare )(
          the_node,
          _RBTree_First( the_rbtree, dir )
        );

        if (
          ( dir == RBT_LEFT && _RBTree_Is_lesser( compare_result ) )
            || ( dir == RBT_RIGHT && !_RBTree_Is_lesser( compare_result ) )
        ) {
          the_rbtree->first[ dir ] = the_node;
        }

        break;
      } else {
        iter_node = iter_node->child[ dir ];
      }
    } /* while(iter_node) */

    /* verify red-black properties */
    _RBTree_Validate_insert( the_node );
  }

  return (RBTree_Node *) 0;
}

RBTree_Node *rbtree_next(
  const RBTree_Node *node,
  RBTree_Direction   dir
)
{
  RBTree_Direction opp_dir = _RBTree_Opposite_direction( dir );
  RBTree_Node     *current = node->child[ dir ];
  RBTree_Node     *next = NULL;

  if ( current != NULL ) {
    next = current;

    while ( ( current = current->child[ opp_dir ] ) != NULL ) {
      next = current;
    }
  } else {
    RBTree_Node *parent = node->parent;

    if ( parent->parent && node == parent->child[ opp_dir ] ) {
      next = parent;
    } else {
      while ( parent->parent && node == parent->child[ dir ] ) {
        node = parent;
        parent = parent->parent;
      }

      if ( parent->parent ) {
        next = parent;
      }
    }
  }

  return next;
}

RBTree_Node *rbtree_find(
  const RBTree_Control *the_rbtree,
  const RBTree_Node    *the_node,
  RBTree_Compare       compare,
  bool                 is_unique
)
{
  RBTree_Node *iter_node = _RBTree_Root( the_rbtree );
  RBTree_Node *found = NULL;

  while ( iter_node != NULL ) {
    RBTree_Compare_result compare_result =
      ( *compare )( the_node, iter_node );

    if ( _RBTree_Is_equal( compare_result ) ) {
      found = iter_node;

      if ( is_unique )
        break;
    }

    if ( _RBTree_Is_greater( compare_result ) ) {
      iter_node = _RBTree_Right( iter_node );
    } else {
      iter_node = _RBTree_Left( iter_node );
    }
  }

  return found;
}

void rbtree_iterate(
  const RBTree_Control *rbtree,
  RBTree_Direction      dir,
  RBTree_Visitor        visitor,
  void                 *visitor_arg
)
{
  const RBTree_Node *current = _RBTree_Minimum( rbtree );
  bool               stop = false;

  while ( !stop && current != NULL ) {
    stop = ( *visitor )( current, dir, visitor_arg );
    current = rbtree_next( current, dir );
  }
}
