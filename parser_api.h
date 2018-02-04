#include "tree.h"


class ExternalCompilerContext {
public:
  bool parse(const char* code,ScopeNode* parent, Node*** nodes, size_t* len);
  virtual ~ExternalCompilerContext(){};
};


ExternalCompilerContext* compiler_new();