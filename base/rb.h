/*
 *  Copyright (c) 2010 Gedare Bloom.
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifndef BASE_RBTREE_H_
#define BASE_RBTREE_H_

#include <stdbool.h>

#include "base/assert.h"
#include "base/tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup RTEMSScoreRBTree Red-Black Tree Handler
 *
 * @ingroup RTEMSScore
 *
 * @brief This group contains the Red-Black Tree Handler implementation.
 *
 * The Red-Black Tree Handler is used to manage sets of entities.  This handler
 * provides two data structures.  The rbtree Node data structure is included
 * as the first part of every data structure that will be placed on
 * a RBTree.  The second data structure is rbtree Control which is used
 * to manage a set of rbtree Nodes.
 *
 * @{
 */

struct RBTree_Control;

/**
 * @brief Red-black tree node.
 *
 * This is used to manage each node (element) which is placed on a red-black
 * tree.
 */
typedef struct RBTree_Node {
  RB_ENTRY(RBTree_Node) Node;
} RBTree_Node;

/**
 * @brief Red-black tree control.
 *
 * This is used to manage a red-black tree.  A red-black tree consists of a
 * tree of zero or more nodes.
 */
typedef RB_HEAD(RBTree_Control, RBTree_Node) RBTree_Control;

/**
 * @brief Initializer for an empty red-black tree with designator @a name.
 */
#define RBTREE_INITIALIZER_EMPTY( name ) \
  RB_INITIALIZER( name )

/**
 * @brief Definition for an empty red-black tree with designator @a name.
 */
#define RBTREE_DEFINE_EMPTY( name ) \
  RBTree_Control name = RBTREE_INITIALIZER_EMPTY( name )

/**
 * @brief Sets a red-black tree node as off-tree.
 *
 * Do not use this function on nodes which are a part of a tree.
 *
 * @param[out] the_node The node to set off-tree.
 *
 * @see _RBTree_Is_node_off_tree().
 */
static inline void _RBTree_Set_off_tree( RBTree_Node *the_node )
{
  RB_COLOR( the_node, Node ) = -1;
}

/**
 * @brief Checks if this red-black tree node is off-tree.
 *
 * @param the_node The node to test.
 *
 * @retval true The node is not a part of a tree (off-tree).
 * @retval false The node is part of a tree.
 *
 * @see _RBTree_Set_off_tree().
 */
static inline bool _RBTree_Is_node_off_tree(
  const RBTree_Node *the_node
)
{
  return RB_COLOR( the_node, Node ) == -1;
}

/**
 * @brief Rebalances the red-black tree after insertion of the node.
 *
 * @param[in, out] the_rbtree The red-black tree control.
 * @param[in, out] the_node The most recently inserted node.
 */
void _RBTree_Insert_color(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node
);

/**
 * @brief Initializes a red-black tree node.
 *
 * In debug configurations, the node is set off tree.  In all other
 * configurations, this function does nothing.
 *
 * @param[out] the_node The red-black tree node to initialize.
 */
static inline void _RBTree_Initialize_node( RBTree_Node *the_node )
{
#if !defined(NODEBUG)
  _RBTree_Set_off_tree( the_node );
#else
  (void) the_node;
#endif
}

/**
 * @brief Adds a child node to a parent node.
 *
 * @param child The child node.
 * @param[out] parent The parent node.
 * @param[out] link The child node link of the parent node.
 */
static inline void _RBTree_Add_child(
  RBTree_Node  *child,
  RBTree_Node  *parent,
  RBTree_Node **link
)
{
  ASSERT_TRUE( _RBTree_Is_node_off_tree( child ) );
  RB_SET( child, parent, Node );
  *link = child;
}

/**
 * @brief Inserts the node into the red-black tree using the specified parent
 * node and link.
 *
 * @param[in, out] the_rbtree The red-black tree control.
 * @param[in, out] the_node The node to insert.
 * @param[out] parent The parent node.
 * @param[out] link The child node link of the parent node.
 *
 * @code
 * #include <rtems/score/rbtree.h>
 *
 * typedef struct {
 *   int value;
 *   RBTree_Node Node;
 * } Some_Node;
 *
 * bool _Some_Less(
 *   const RBTree_Node *a,
 *   const RBTree_Node *b
 * )
 * {
 *   const Some_Node *aa = RTEMS_CONTAINER_OF( a, Some_Node, Node );
 *   const Some_Node *bb = RTEMS_CONTAINER_OF( b, Some_Node, Node );
 *
 *   return aa->value < bb->value;
 * }
 *
 * void _Some_Insert(
 *   RBTree_Control *the_rbtree,
 *   Some_Node      *the_node
 * )
 * {
 *   RBTree_Node **link = _RBTree_Root_reference( the_rbtree );
 *   RBTree_Node  *parent = NULL;
 *
 *   while ( *link != NULL ) {
 *     parent = *link;
 *
 *     if ( _Some_Less( &the_node->Node, parent ) ) {
 *       link = _RBTree_Left_reference( parent );
 *     } else {
 *       link = _RBTree_Right_reference( parent );
 *     }
 *   }
 *
 *   _RBTree_Insert_with_parent( the_rbtree, &the_node->Node, parent, link );
 * }
 * @endcode
 */
static inline void _RBTree_Insert_with_parent(
  RBTree_Control  *the_rbtree,
  RBTree_Node     *the_node,
  RBTree_Node     *parent,
  RBTree_Node    **link
)
{
  _RBTree_Add_child( the_node, parent, link );
  _RBTree_Insert_color( the_rbtree, the_node );
}

/**
 * @brief Extracts (removes) the node from the red-black tree.
 *
 * This function does not set the node off-tree.  In the case this is desired, then
 * call _RBTree_Set_off_tree() after the extraction.
 *
 * In the case the node to extract is not a node of the tree, then this function
 * yields unpredictable results.
 *
 * @param[in, out] the_rbtree The red-black tree control.
 * @param[out] the_node The node to extract.
 */
void _RBTree_Extract(
  RBTree_Control *the_rbtree,
  RBTree_Node *the_node
);

/**
 * @brief Returns a pointer to root node of the red-black tree.
 *
 * The root node may change after insert or extract operations.
 *
 * @param the_rbtree The red-black tree control.
 *
 * @retval root The root node.
 * @retval NULL The tree is empty.
 *
 * @see _RBTree_Is_root().
 */
static inline RBTree_Node *_RBTree_Root(
  const RBTree_Control *the_rbtree
)
{
  return RB_ROOT( the_rbtree );
}

/**
 * @brief Returns a reference to the root pointer of the red-black tree.
 *
 * @param the_rbtree The red-black tree control.
 *
 * @retval pointer Pointer to the root node.
 * @retval NULL The tree is empty.
 */
static inline RBTree_Node **_RBTree_Root_reference(
  RBTree_Control *the_rbtree
)
{
  return &RB_ROOT( the_rbtree );
}

/**
 * @brief Returns a constant reference to the root pointer of the red-black tree.
 *
 * @param the_rbtree The red-black tree control.
 *
 * @retval pointer Pointer to the root node.
 * @retval NULL The tree is empty.
 */
static inline RBTree_Node * const *_RBTree_Root_const_reference(
  const RBTree_Control *the_rbtree
)
{
  return &RB_ROOT( the_rbtree );
}

/**
 * @brief Returns a pointer to the parent of this node.
 *
 * The node must have a parent, thus it is invalid to use this function for the
 * root node or a node that is not part of a tree.  To test for the root node
 * compare with _RBTree_Root() or use _RBTree_Is_root().
 *
 * @param the_node The node of interest.
 *
 * @retval parent The parent of this node.
 * @retval undefined The node is the root node or not part of a tree.
 */
static inline RBTree_Node *_RBTree_Parent(
  const RBTree_Node *the_node
)
{
  return RB_PARENT( the_node, Node );
}

/**
 * @brief Returns pointer to the left of this node.
 *
 * This function returns a pointer to the left node of this node.
 *
 * @param the_node is the node to be operated upon.
 *
 * @return This method returns the left node on the rbtree.
 */
static inline RBTree_Node *_RBTree_Left(
  const RBTree_Node *the_node
)
{
  return RB_LEFT( the_node, Node );
}

/**
 * @brief Returns a reference to the left child pointer of the red-black tree
 * node.
 *
 * @param the_node is the node to be operated upon.
 *
 * @return This method returns a reference to the left child pointer on the rbtree.
 */
static inline RBTree_Node **_RBTree_Left_reference(
  RBTree_Node *the_node
)
{
  return &RB_LEFT( the_node, Node );
}

/**
 * @brief Returns pointer to the right of this node.
 *
 * This function returns a pointer to the right node of this node.
 *
 * @param the_node is the node to be operated upon.
 *
 * @return This method returns the right node on the rbtree.
 */
static inline RBTree_Node *_RBTree_Right(
  const RBTree_Node *the_node
)
{
  return RB_RIGHT( the_node, Node );
}

/**
 * @brief Returns a reference to the right child pointer of the red-black tree
 * node.
 *
 * @param the_node is the node to be operated upon.
 *
 * @return This method returns a reference to the right child pointer on the rbtree.
 */
static inline RBTree_Node **_RBTree_Right_reference(
  RBTree_Node *the_node
)
{
  return &RB_RIGHT( the_node, Node );
}

/**
 * @brief Checks if the RBTree is empty.
 *
 * This function returns true if there are no nodes on @a the_rbtree and
 * false otherwise.
 *
 * @param the_rbtree is the rbtree to be operated upon.
 *
 * @retval true There are no nodes on @a the_rbtree.
 * @retval false There are nodes on @a the_rbtree.
 */
static inline bool _RBTree_Is_empty(
  const RBTree_Control *the_rbtree
)
{
  return RB_EMPTY( the_rbtree );
}

/**
 * @brief Checks if this node is the root node of a red-black tree.
 *
 * The root node may change after insert or extract operations.  In case the
 * node is not a node of a tree, then this function yields unpredictable
 * results.
 *
 * @param the_node The node of interest.
 *
 * @retval true @a the_node is the root node.
 * @retval false @a the_node is not the root node.
 *
 * @see _RBTree_Root().
 */
static inline bool _RBTree_Is_root(
  const RBTree_Node *the_node
)
{
  return _RBTree_Parent( the_node ) == NULL;
}

/**
 * @brief Initializes this RBTree as empty.
 *
 * This routine initializes @a the_rbtree to contain zero nodes.
 *
 * @param[out] the_rbtree The rbtree to initialize.
 */
static inline void _RBTree_Initialize_empty(
  RBTree_Control *the_rbtree
)
{
  RB_INIT( the_rbtree );
}

/**
 * @brief Initializes this red-black tree to contain exactly the specified
 * node.
 *
 * @param[out] the_rbtree The red-black tree control.
 * @param[out] the_node The one and only node.
 */
static inline void _RBTree_Initialize_one(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node
)
{
  ASSERT_TRUE( _RBTree_Is_node_off_tree( the_node ) );
  RB_ROOT( the_rbtree ) = the_node;
  RB_PARENT( the_node, Node ) = NULL;
  RB_LEFT( the_node, Node ) = NULL;
  RB_RIGHT( the_node, Node ) = NULL;
  RB_COLOR( the_node, Node ) = RB_BLACK;
}

/**
 * @brief Returns the minimum node of the red-black tree.
 *
 * @param the_rbtree The red-black tree control.
 *
 * @retval node The minimum node.
 * @retval NULL The red-black tree is empty.
 */
RBTree_Node *_RBTree_Minimum( const RBTree_Control *the_rbtree );

/**
 * @brief Returns the maximum node of the red-black tree.
 *
 * @param the_rbtree The red-black tree control.
 *
 * @retval node The maximum node.
 * @retval NULL The red-black tree is empty.
 */
RBTree_Node *_RBTree_Maximum( const RBTree_Control *the_rbtree );

/**
 * @brief Returns the predecessor of a node.
 *
 * @param node is the node.
 *
 * @retval node The predecessor node.
 * @retval NULL The predecessor does not exist.
 */
RBTree_Node *_RBTree_Predecessor( const RBTree_Node *node );

/**
 * @brief Returns the successor of a node.
 *
 * @param node is the node.
 *
 * @retval node The successor node.
 * @retval NULL The successor does not exist.
 */
RBTree_Node *_RBTree_Successor( const RBTree_Node *node );

/**
 * @brief Replaces a node in the red-black tree without a rebalance.
 *
 * @param[in, out] the_rbtree The red-black tree control.
 * @param victim The victim node.
 * @param[out] replacement The replacement node.
 */
void _RBTree_Replace_node(
  RBTree_Control *the_rbtree,
  RBTree_Node    *victim,
  RBTree_Node    *replacement
);

/**
 * @brief Inserts the node into the red-black tree.
 *
 * @param[in, out] the_rbtree The red-black tree control.
 * @param[out] the_node The node to insert.
 * @param key The key of the node to insert.  This key must be equal to the key
 *   stored in the node to insert.  The separate key parameter is provided for
 *   two reasons.  Firstly, it allows to share the less operator with
 *   _RBTree_Find_inline().  Secondly, the compiler may generate better code if
 *   the key is stored in a local variable.
 * @param less Must return true if the specified key is less than the key of
 *   the node, otherwise false.
 *
 * @retval true The inserted node is the new minimum node according to the
 *   specified less order function.
 * @retval false The inserted node is not the new minimum node according to the
 *   specified less order function.
 */
static inline bool _RBTree_Insert_inline(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node,
  const void     *key,
  bool         ( *less )( const void *, const RBTree_Node * )
)
{
  RBTree_Node **link;
  RBTree_Node  *parent;
  bool          is_new_minimum;

  link = _RBTree_Root_reference( the_rbtree );
  parent = NULL;
  is_new_minimum = true;

  while ( *link != NULL ) {
    parent = *link;

    if ( ( *less )( key, parent ) ) {
      link = _RBTree_Left_reference( parent );
    } else {
      link = _RBTree_Right_reference( parent );
      is_new_minimum = false;
    }
  }

  _RBTree_Add_child( the_node, parent, link );
  _RBTree_Insert_color( the_rbtree, the_node );
  return is_new_minimum;
}

/**
 * @brief Finds an object in the red-black tree with the specified key.
 *
 * @param the_rbtree The red-black tree control.
 * @param key The key to look after.
 * @param equal Must return true if the specified key equals the key of the
 *   node, otherwise false.
 * @param less Must return true if the specified key is less than the key of
 *   the node, otherwise false.
 * @param map In case a node with the specified key is found, then this
 *   function is called to map the node to the object returned.  Usually it
 *   performs some offset operation via RTEMS_CONTAINER_OF() to map the node to
 *   its containing object.  Thus, the return type is a void pointer and not a
 *   red-black tree node.
 *
 * @retval object An object with the specified key.
 * @retval NULL No object with the specified key exists in the red-black tree.
 */
static inline void *_RBTree_Find_inline(
  const RBTree_Control *the_rbtree,
  const void           *key,
  bool               ( *equal )( const void *, const RBTree_Node * ),
  bool               ( *less )( const void *, const RBTree_Node * ),
  void              *( *map )( RBTree_Node * )
)
{
  RBTree_Node * const *link;
  RBTree_Node         *parent;

  link = _RBTree_Root_const_reference( the_rbtree );
  parent = NULL;

  while ( *link != NULL ) {
    parent = *link;

    if ( ( *equal )( key, parent ) ) {
      return ( *map )( parent );
    } else if ( ( *less )( key, parent ) ) {
      link = _RBTree_Left_reference( parent );
    } else {
      link = _RBTree_Right_reference( parent );
    }
  }

  return NULL;
}

/**
 * @brief Returns the container of the first node of the specified red-black
 * tree in postorder.
 *
 * Postorder traversal may be used to delete all nodes of a red-black tree.
 *
 * @param the_rbtree The red-black tree control.
 * @param offset The offset to the red-black tree node in the container structure.
 *
 * @retval container The container of the first node of the specified red-black
 *   tree in postorder.
 * @retval NULL The red-black tree is empty.
 *
 * @see _RBTree_Postorder_next().
 *
 * @code
 * #include <rtems/score/rbtree.h>
 *
 * typedef struct {
 *   int         data;
 *   RBTree_Node Node;
 * } Container_Control;
 *
 * void visit( Container_Control *the_container );
 *
 * void postorder_traversal( RBTree_Control *the_rbtree )
 * {
 *   Container_Control *the_container;
 *
 *   the_container = _RBTree_Postorder_first(
 *     the_rbtree,
 *     offsetof( Container_Control, Node )
 *   );
 *
 *   while ( the_container != NULL ) {
 *     visit( the_container );
 *
 *     the_container = _RBTree_Postorder_next(
 *       &the_container->Node,
 *       offsetof( Container_Control, Node )
 *     );
 *   }
 * }
 * @endcode
 */
void *_RBTree_Postorder_first(
  const RBTree_Control *the_rbtree,
  size_t                offset
);

/**
 * @brief Returns the container of the next node in postorder.
 *
 * @param the_node The red-black tree node.  The node must not be NULL.
 * @param offset The offset to the red-black tree node in the container structure.
 *
 * @retval container The container of the next node in postorder.
 * @retval NULL The node is NULL or there is no next node in postorder.
 *
 * @see _RBTree_Postorder_first().
 */
void *_RBTree_Postorder_next(
  const RBTree_Node *the_node,
  size_t             offset
);


/**
 * @typedef rbtree_node
 *
 * A node that can be manipulated in the rbtree.
 */
typedef RBTree_Node rbtree_node;

/**
 * @typedef rbtree_control
 *
 * The rbtree's control anchors the rbtree.
 */
typedef RBTree_Control rbtree_control;

/**
 * @brief Integer type for compare results.
 *
 * The type is large enough to represent pointers and 32-bit signed integers.
 *
 * @see rbtree_compare.
 */
typedef long rbtree_compare_result;

/**
 * @brief Compares two red-black tree nodes.
 *
 * @param[in] first The first node.
 * @param[in] second The second node.
 *
 * @retval positive The key value of the first node is greater than the one of
 *   the second node.
 * @retval 0 The key value of the first node is equal to the one of the second
 *   node.
 * @retval negative The key value of the first node is less than the one of the
 *   second node.
 */
typedef rbtree_compare_result ( *rbtree_compare )(
  const RBTree_Node *first,
  const RBTree_Node *second
);

/**
 * @brief RBTree initializer for an empty rbtree with designator @a name.
 */
#define RTEMS_RBTREE_INITIALIZER_EMPTY(name) \
  RBTREE_INITIALIZER_EMPTY(name)

/**
 * @brief RBTree definition for an empty rbtree with designator @a name.
 */
#define RTEMS_RBTREE_DEFINE_EMPTY(name) \
  RBTREE_DEFINE_EMPTY(name)

/**
 * @brief Initialize a RBTree header.
 *
 * This routine initializes @a the_rbtree structure to manage the
 * contiguous array of @a number_nodes nodes which starts at
 * @a starting_address.  Each node is of @a node_size bytes.
 *
 * @param[in] the_rbtree is the pointer to rbtree header
 * @param[in] compare The node compare function.
 * @param[in] starting_address is the starting address of first node
 * @param[in] number_nodes is the number of nodes in rbtree
 * @param[in] node_size is the size of node in bytes
 * @param[in] is_unique If true, then reject nodes with a duplicate key, else
 *   otherwise.
 */
void rbtree_initialize(
  rbtree_control *the_rbtree,
  rbtree_compare  compare,
  void                 *starting_address,
  size_t                number_nodes,
  size_t                node_size,
  bool                  is_unique
);

/**
 * @brief Initialize this RBTree as Empty
 *
 * This routine initializes @a the_rbtree to contain zero nodes.
 */
static inline void rbtree_initialize_empty(
  rbtree_control *the_rbtree
)
{
  _RBTree_Initialize_empty( the_rbtree );
}

/**
 * @brief Set off RBtree.
 *
 * This function sets the next and previous fields of the @a node to NULL
 * indicating the @a node is not part of any rbtree.
 */
static inline void rbtree_set_off_tree(
  rbtree_node *node
)
{
  _RBTree_Set_off_tree( node );
}

/**
 * @brief Is the Node off RBTree.
 *
 * This function returns true if the @a node is not on a rbtree. A @a node is
 * off rbtree if the next and previous fields are set to NULL.
 */
static inline bool rbtree_is_node_off_tree(
  const rbtree_node *node
)
{
  return _RBTree_Is_node_off_tree( node );
}

/**
 * @brief Return pointer to RBTree root.
 *
 * This function returns a pointer to the root node of @a the_rbtree.
 */
static inline rbtree_node *rbtree_root(
  const rbtree_control *the_rbtree
)
{
  return _RBTree_Root( the_rbtree );
}

/**
 * @copydoc _RBTree_Minimum()
 */
static inline rbtree_node *rbtree_min(
  const rbtree_control *the_rbtree
)
{
  return _RBTree_Minimum( the_rbtree );
}

/**
 * @copydoc _RBTree_Maximum()
 */
static inline rbtree_node *rbtree_max(
  const rbtree_control *the_rbtree
)
{
  return _RBTree_Maximum( the_rbtree );
}

/**
 * @brief Return pointer to the left child node from this node.
 *
 * This function returns a pointer to the left child node of @a the_node.
 */
static inline rbtree_node *rbtree_left(
  const rbtree_node *the_node
)
{
  return _RBTree_Left( the_node );
}

/**
 * @brief Return pointer to the right child node from this node.
 *
 * This function returns a pointer to the right child node of @a the_node.
 */
static inline rbtree_node *rbtree_right(
  const rbtree_node *the_node
)
{
  return _RBTree_Right( the_node );
}

/**
 * @copydoc _RBTree_Parent()
 */
static inline rbtree_node *rbtree_parent(
  const rbtree_node *the_node
)
{
  return _RBTree_Parent( the_node );
}

/**
 * @brief Is the RBTree empty.
 *
 * This function returns true if there a no nodes on @a the_rbtree and
 * false otherwise.
 */
static inline bool rbtree_is_empty(
  const rbtree_control *the_rbtree
)
{
  return _RBTree_Is_empty( the_rbtree );
}

/**
 * @brief Is this the minimum node on the RBTree.
 *
 * This function returns true if @a the_node is the min node on @a the_rbtree 
 * and false otherwise.
 */
static inline bool rbtree_is_min(
  const rbtree_control *the_rbtree,
  const rbtree_node *the_node
)
{
  return rbtree_min( the_rbtree ) == the_node;
}

/**
 * @brief Is this the maximum node on the RBTree.
 *
 * This function returns true if @a the_node is the max node on @a the_rbtree 
 * and false otherwise.
 */
static inline bool rbtree_is_max(
  const rbtree_control *the_rbtree,
  const rbtree_node *the_node
)
{
  return rbtree_max( the_rbtree ) == the_node;
}

/**
 * @copydoc _RBTree_Is_root()
 */
static inline bool rbtree_is_root(
  const rbtree_node *the_node
)
{
  return _RBTree_Is_root( the_node );
}

static inline bool rbtree_is_equal(
  rbtree_compare_result compare_result
)
{
  return compare_result == 0;
}

static inline bool rbtree_is_greater(
  rbtree_compare_result compare_result
)
{
  return compare_result > 0;
}

static inline bool rbtree_is_lesser(
  rbtree_compare_result compare_result
)
{
  return compare_result < 0;
}

/**
 * @brief Tries to find a node for the specified key in the tree.
 *
 * @param[in] the_rbtree The red-black tree control.
 * @param[in] the_node A node specifying the key.
 * @param[in] compare The node compare function.
 * @param[in] is_unique If true, then return the first node with a key equal to
 *   the one of the node specified if it exits, else return the last node if it
 *   exists.
 *
 * @retval node A node corresponding to the key.  If the tree is not unique
 * and contains duplicate keys, the set of duplicate keys acts as FIFO.
 * @retval NULL No node exists in the tree for the key.
 */
rbtree_node* rbtree_find(
  const rbtree_control *the_rbtree,
  const rbtree_node    *the_node,
  rbtree_compare              compare,
  bool                        is_unique
);

/**
 * @copydoc _RBTree_Predecessor()
 */
static inline rbtree_node* rbtree_predecessor(
  const rbtree_node *node
)
{
  return _RBTree_Predecessor( node );
}

/**
 * @copydoc _RBTree_Successor()
 */
static inline rbtree_node* rbtree_successor(
  const rbtree_node *node
)
{
  return _RBTree_Successor( node );
}

/**
 * @copydoc _RBTree_Extract()
 */
static inline void rbtree_extract(
  rbtree_control *the_rbtree,
  rbtree_node *the_node
)
{
  _RBTree_Extract( the_rbtree, the_node );
}

/**
 * @brief Gets a node with the minimum key value from the red-black tree.
 *
 * This function extracts a node with the minimum key value from
 * tree and returns a pointer to that node if it exists.  In case multiple
 * nodes with a minimum key value exist, then they are extracted in FIFO order.
 *
 * @param[in] the_rbtree The red-black tree control.
 *
 * @retval NULL The tree is empty.
 * @retval node A node with the minimal key value on the tree.
 */
static inline rbtree_node *rbtree_get_min(
  rbtree_control *the_rbtree
)
{
  rbtree_node *the_node = rbtree_min( the_rbtree );

  if ( the_node != NULL ) {
    rbtree_extract( the_rbtree, the_node );
  }

  return the_node;
}

/**
 * @brief Gets a node with the maximal key value from the red-black tree.
 *
 * This function extracts a node with the maximum key value from tree and
 * returns a pointer to that node if it exists.  In case multiple nodes with a
 * maximum key value exist, then they are extracted in LIFO order.
 *
 * @param[in] the_rbtree The red-black tree control.
 *
 * @retval NULL The tree is empty.
 * @retval node A node with the maximal key value on the tree.
 */
static inline rbtree_node *rbtree_get_max(
  rbtree_control *the_rbtree
)
{
  rbtree_node *the_node = rbtree_max( the_rbtree );

  if ( the_node != NULL ) {
    rbtree_extract( the_rbtree, the_node );
  }

  return the_node;
}

/**
 * @brief Peek at the min node on a rbtree.
 *
 * This function returns a pointer to the min node from @a the_rbtree 
 * without changing the tree.  If @a the_rbtree is empty, 
 * then NULL is returned.
 */
static inline rbtree_node *rbtree_peek_min(
  const rbtree_control *the_rbtree
)
{
  return rbtree_min( the_rbtree );
}

/**
 * @brief Peek at the max node on a rbtree.
 *
 * This function returns a pointer to the max node from @a the_rbtree 
 * without changing the tree.  If @a the_rbtree is empty, 
 * then NULL is returned.
 */
static inline rbtree_node *rbtree_peek_max(
  const rbtree_control *the_rbtree
)
{
  return rbtree_max( the_rbtree );
}

/**
 * @brief Inserts the node into the red-black tree.
 *
 * In case the node is already a node of a tree, then this function yields
 * unpredictable results.
 *
 * @param[in] the_rbtree The red-black tree control.
 * @param[in] the_node The node to insert.
 * @param[in] compare The node compare function.
 * @param[in] is_unique If true, then reject nodes with a duplicate key, else
 *   insert nodes in FIFO order in case the key value is equal to existing nodes.
 *
 * @retval NULL Successfully inserted.
 * @retval existing_node This is a unique insert and there exists a node with
 *   an equal key in the tree already.
 */
rbtree_node *rbtree_insert(
  RBTree_Control *the_rbtree,
  RBTree_Node    *the_node,
  rbtree_compare  compare,
  bool            is_unique
);


/**
 * @brief Appends the node to the red-black tree.
 *
 * The appended node is the new maximum node of the tree.  The caller shall
 * ensure that the appended node is indeed the maximum node with respect to the
 * tree order.
 *
 * @param[in, out] the_rbtree is the red-black tree control.
 *
 * @param the_node[out] is the node to append.
 */
void _RBTree_Append( RBTree_Control *the_rbtree, RBTree_Node *the_node );

/**
 * @brief Prepends the node to the red-black tree.
 *
 * The prepended node is the new minimum node of the tree.  The caller shall
 * ensure that the prepended node is indeed the minimum node with respect to the
 * tree order.
 *
 * @param[in, out] the_rbtree is the red-black tree control.
 *
 * @param the_node[out] is the node to prepend.
 */
void _RBTree_Prepend( RBTree_Control *the_rbtree, RBTree_Node *the_node );

/**
 * @brief Red-black tree visitor.
 *
 * @param[in] node The node.
 * @param[in] visitor_arg The visitor argument.
 *
 * @retval true Stop the iteration.
 * @retval false Continue the iteration.
 *
 * @see _RBTree_Iterate().
 */
typedef bool (*RBTree_Visitor)(
  const RBTree_Node *node,
  void *visitor_arg
);
typedef RBTree_Visitor rbtree_visitor_t;

/**
 * @brief Red-black tree iteration.
 *
 * @param rbtree The red-black tree.
 * @param visitor The visitor.
 * @param visitor_arg The visitor argument.
 */
void _RBTree_Iterate(
  const RBTree_Control *rbtree,
  RBTree_Visitor visitor,
  void *visitor_arg
);

static inline void rbtree_iterate(
  const rbtree_control *rbtree,
  rbtree_visitor_t visitor,
  void *arg
)
{
  _RBTree_Iterate( rbtree, visitor, arg );
}


#ifdef __cplusplus
}
#endif

#endif
/* end of include file */