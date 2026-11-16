// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_indexbuf_hpp
#define endlesstunnel_indexbuf_hpp

#include "common.hpp"

/* Represents an index buffer (IBO). */
class IndexBuf {
 public:
  IndexBuf(GLushort *data, int dataSizeBytes);
  ~IndexBuf();

  void BindBuffer();
  void UnbindBuffer();
  int GetCount() { return mCount; }

 private:
  GLuint mIbo;
  int mCount;
};

#endif
