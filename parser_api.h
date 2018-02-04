#include "tree.h"


class CompilerContext {
public:
  bool parse(const char* code,ScopeNode* parent, Node*** nodes, size_t* len);
  virtual ~CompilerContext(){};
};


CompilerContext* compiler_new();