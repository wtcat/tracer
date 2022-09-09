/*
 *  Copyright (c) 2010 Gedare Bloom.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */
/*
 *  Copyright (c) 2022 wtcat.
 */
#include <stdint.h>

#include "base/rb.h"
#include "base/assert.h"

RB_GENERATE_REMOVE_COLOR( RBTree_Control, RBTree_Node, Node, static )
RB_GENERATE_REMOVE( RBTree_Control, RBTree_Node, Node, static )
RB_GENERATE_NEXT( RBTree_Control, RBTree_Node, Node, static )
RB_GENERATE_INSERT_COLOR( RBTree_Control, RBTree_Node, Node, static inline )

void _RBTree_Insert_color(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node
)
{
  RBTree_Control_RB_INSERT_COLOR( the_rbtree, the_node );
}

RBTree_Node *_RBTree_Successor( const RBTree_Node *node )
{
  return RB_NEXT( RBTree_Control, NULL, ( RBTree_Node *) node );
}

static inline void *_Addresses_Add_offset (
  const void *base,
  uintptr_t   offset
)
{
  return (void *)((uintptr_t)base + offset);
}

#if !defined(NODEBUG)
static const RBTree_Node *_RBTree_Find_root( const RBTree_Node *the_node )
{
  while ( true ) {
    const RBTree_Node *potential_root;

    potential_root = the_node;
    the_node = _RBTree_Parent( the_node );

    if ( the_node == NULL ) {
      return potential_root;
    }
  }
}
#endif

void _RBTree_Extract(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node
)
{
  ASSERT_TRUE( _RBTree_Find_root( the_node ) == _RBTree_Root( the_rbtree ) );
  RB_REMOVE( RBTree_Control, the_rbtree, the_node );
  _RBTree_Initialize_node( the_node );
}

RBTree_Node *_RBTree_Minimum( const RBTree_Control *tree )
{
  RBTree_Node *parent;
  RBTree_Node *node;

  parent = NULL;
  node = _RBTree_Root( tree );

  while ( node != NULL ) {
    parent = node;
    node = _RBTree_Left( node );
  }

  return parent;
}

RBTree_Node *_RBTree_Maximum( const RBTree_Control *tree )
{
  RBTree_Node *parent;
  RBTree_Node *node;

  parent = NULL;
  node = _RBTree_Root( tree );

  while ( node != NULL ) {
    parent = node;
    node = _RBTree_Right( node );
  }

  return parent;
}

void _RBTree_Iterate(
  const RBTree_Control *rbtree,
  RBTree_Visitor        visitor,
  void                 *visitor_arg
)
{
  const RBTree_Node *current = _RBTree_Minimum( rbtree );
  bool               stop = false;

  while ( !stop && current != NULL ) {
    stop = ( *visitor )( current, visitor_arg );

    current = _RBTree_Successor( current );
  }
}

rbtree_node *rbtree_insert(
  rbtree_control *the_rbtree,
  rbtree_node    *the_node,
  rbtree_compare  compare,
  bool                  is_unique
)
{
  rbtree_node **which = _RBTree_Root_reference( the_rbtree );
  rbtree_node  *parent = NULL;

  while ( *which != NULL ) {
    rbtree_compare_result compare_result;

    parent = *which;
    compare_result = ( *compare )( the_node, parent );

    if ( is_unique && rbtree_is_equal( compare_result ) ) {
      return parent;
    }

    if ( rbtree_is_lesser( compare_result ) ) {
      which = _RBTree_Left_reference( parent );
    } else {
      which = _RBTree_Right_reference( parent );
    }
  }

  _RBTree_Initialize_node( the_node );
  _RBTree_Add_child( the_node, parent, which );
  _RBTree_Insert_color( the_rbtree, the_node );

  return NULL;
}

rbtree_node *rbtree_find(
  const rbtree_control *the_rbtree,
  const rbtree_node    *the_node,
  rbtree_compare        compare,
  bool                  is_unique
)
{
  rbtree_node *iter_node = rbtree_root( the_rbtree );
  rbtree_node *found = NULL;

  while ( iter_node != NULL ) {
    rbtree_compare_result compare_result =
      ( *compare )( the_node, iter_node );

    if ( rbtree_is_equal( compare_result ) ) {
      found = iter_node;

      if ( is_unique )
        break;
    }

    if ( rbtree_is_greater( compare_result ) ) {
      iter_node = rbtree_right( iter_node );
    } else {
      iter_node = rbtree_left( iter_node );
    }
  }

  return found;
}

void rbtree_initialize(
  rbtree_control *the_rbtree,
  rbtree_compare  compare,
  void                 *starting_address,
  size_t                number_nodes,
  size_t                node_size,
  bool                  is_unique
)
{
  size_t             count;
  rbtree_node *next;

  /* could do sanity checks here */
  rbtree_initialize_empty( the_rbtree );

  count = number_nodes;
  next = starting_address;

  while ( count-- ) {
    rbtree_insert( the_rbtree, next, compare, is_unique );
    next = (rbtree_node *) _Addresses_Add_offset( next, node_size );
  }
}

