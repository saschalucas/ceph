// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */


#ifndef __FILER_H
#define __FILER_H

/*** Filer
 *
 * stripe file ranges onto objects.
 * build list<ObjectExtent> for the objecter or objectcacher.
 *
 * also, provide convenience methods that call objecter for you.
 *
 * "files" are identified by ino. 
 */

#include <set>
#include <map>
using namespace std;

#include <ext/hash_map>
#include <ext/rope>
using namespace __gnu_cxx;

#include "include/types.h"

#include "osd/OSDMap.h"
#include "Objecter.h"

class Context;
class Messenger;
class OSDMap;


/**** Filer interface ***/

class Filer {
  Objecter   *objecter;
  
  // probes
  struct Probe {
    inode_t inode;
    off_t from;
    off_t *end;
    Context *onfinish;
    
    list<ObjectExtent> probing;
    off_t probing_len;
    
    map<object_t, off_t> known;
    map<object_t, tid_t> ops;

    Probe(inode_t &i, off_t f, off_t *e, Context *c) : 
      inode(i), from(f), end(e), onfinish(c), probing_len(0) {}
  };
  
  class C_Probe;
  //friend class C_Probe;  

  void _probe(Probe *p);
  void _probed(Probe *p, object_t oid, off_t size);

 public:
  Filer(Objecter *o) : objecter(o) {}
  ~Filer() {}

  bool is_active() {
    return objecter->is_active(); // || (oc && oc->is_active());
  }

  /*** async file interface ***/
  int read(inode_t& inode,
           off_t offset, 
           size_t len, 
           bufferlist *bl,   // ptr to data
           Context *onfinish, ExtCap* read_cap=0) {
    // we should always pass a read_cap...this is just
    // to support legacy stuff
    Objecter::OSDRead *rd;
    if (!read_cap)
      rd = new Objecter::OSDRead(bl);
    else
      rd = new Objecter::OSDRead(bl, read_cap);
    file_to_extents(inode, offset, len, rd->extents);
    return objecter->readx(rd, onfinish) > 0 ? 0:-1;
  }

  int write(inode_t& inode,
            off_t offset, 
            size_t len, 
            bufferlist& bl,
            int flags, 
            Context *onack,
            Context *oncommit, ExtCap* write_cap=0,
	    objectrev_t rev=0) {
    Objecter::OSDWrite *wr;
    if (!write_cap) // we should always pass a cap
      wr = new Objecter::OSDWrite(bl);
    else
      wr = new Objecter::OSDWrite(bl, write_cap);
    file_to_extents(inode, offset, len, wr->extents, rev);
    return objecter->modifyx(wr, onack, oncommit) > 0 ? 0:-1;
  }

  int zero(inode_t& inode,
           off_t offset,
           size_t len,
           Context *onack,
           Context *oncommit) {
    Objecter::OSDModify *z = new Objecter::OSDModify(OSD_OP_ZERO);
    file_to_extents(inode, offset, len, z->extents);
    return objecter->modifyx(z, onack, oncommit) > 0 ? 0:-1;
  }

  int remove(inode_t& inode,
	     off_t offset,
	     size_t len,
	     Context *onack,
	     Context *oncommit) {
    Objecter::OSDModify *z = new Objecter::OSDModify(OSD_OP_DELETE);
    file_to_extents(inode, offset, len, z->extents);
    return objecter->modifyx(z, onack, oncommit) > 0 ? 0:-1;
  }

  int probe_fwd(inode_t& inode,
		off_t start_from,
		off_t *end,
		Context *onfinish);


  /***** mapping *****/

  /* map (ino, ono) to an object name
     (to be used on any osd in the proper replica group) */
  /*object_t file_to_object(inodeno_t ino,
                          size_t    _ono) {  
    __uint64_t ono = _ono;
    assert(ino < (1ULL<<OID_INO_BITS));       // legal ino can't be too big
    assert(ono < (1ULL<<OID_ONO_BITS));
    return ono + (ino << OID_ONO_BITS);
  }
  */


  /* map (ino, offset, len) to a (list of) OSDExtents 
     (byte ranges in objects on (primary) osds) */
  void file_to_extents(inode_t inode,
                       off_t offset,
                       size_t len,
                       list<ObjectExtent>& extents,
		       objectrev_t rev=0);
  
};



#endif
