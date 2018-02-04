#include "tree.h"


class ExternalCompilerContext {
public:
  bool parse(const char* code,ScopeNode* parent, Node*** nodes, size_t* len);
  Node* resolve(const char* offset); //Resolves a node at a specific location in text.
  virtual ~ExternalCompilerContext(){};
};


ExternalCompilerContext* compiler_new();