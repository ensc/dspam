/* $Id: tbt.h,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2004 NETWORK DWEEBS CORPORATION

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 *  tbt - Tokenized Binary Tree
 *
 *  The tokenized binary tree provides a binary sorting algorithm based
 *  on statistical probability drift from a neutral 0.5. The key of the
 *  binary  tree is  essentially  abs(0.5-p)). The  tree  can  also  be 
 *  organized based on incremental value.
 */

#ifndef _TBT_H
#define _TBT_H

struct tbt
{
  unsigned long items;
  struct tbt_node *root;
  int type;
};

/*
 *  tbt_node - A Binary Tree Node
 *
 *  A single  node in the  binary tree.  Each node represents  a data 
 *  point (token) in the message being processed.
 * 
 *  delta
 *    The key of the node, is the probability's delta from neutral .5
 *    The delta is calculated as abs(0.5-p)).
 *
 *  probability
 *    The actual probability of the token
 *
 *  token
 *    The CRC (64) of the token
 *
 *  frequency
 *    The number of  times this  token appeared in  the message  being 
 *    processed.
 *
 *  complexity
 *    The complexity, or depth, of the token 
 *
 *  left, right, parent
 *    Pointers to other nodes in the binary tree
 */

struct tbt_node
{
  double delta;
  float probability;
  unsigned long long token;
  unsigned long frequency;
  int complexity;
  struct tbt_node *left;
  struct tbt_node *right;
  struct tbt_node *parent; // Makes life easier
};

struct tbt *	tbt_create	(void);
int		tbt_destroy	(struct tbt *tbt);

struct tbt_node *tbt_first	(struct tbt *tbt);
struct tbt_node *tbt_next	(struct tbt_node *node);

int tbt_add (struct tbt *tbt, 
             double probability,
             unsigned long long token,
             unsigned long frequency,
             int complexity);

#endif /* _TBT_H */
