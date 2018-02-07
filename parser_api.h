#include "tree.h"


class ExternalCompilerContext {
public:
  virtual bool parse(const char* code,ScopeNode* parent, Node*** nodes, size_t* len) = 0;
  virtual Node* resolve(const char* offset) = 0; //Resolves a node at a specific location in text.
  virtual ~ExternalCompilerContext(){};
};


ExternalCompilerContext* compiler_new();
