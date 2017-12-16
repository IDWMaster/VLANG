#ifndef TREE_HEADER
#define TREE_HEADER
#include "libparse/parser.h"
#include <map>
#include <sstream>
#include <vector>


using namespace libparse;


enum NodeType {
  Class, Scope, VariableDeclaration, AssignOp, Constant, BinaryExpression, VariableReference, Goto, Label, UnaryExpression, Function
};
enum ConstantType {
  Integer, String, Character
};
class Node {
public:
  NodeType type;
  Node(NodeType type):type(type) {
    
  }
};

class LabelNode:public Node {
public:
  StringRef name;
  int id;
  LabelNode():Node(Label) {
  }
};



class ScopeNode:public Node {
public:
  ScopeNode* parent;
  std::map<StringRef,Node*> tokens;
  ScopeNode():Node(Scope) {
    parent = 0;
  }
  Node* resolve(const StringRef& name) {
    if(tokens.find(name) == tokens.end()) {
      if(parent) {
	return parent->resolve(name);
      }
      return 0;
    }else {
      return tokens[name];
    }
  }
  bool add(const StringRef& name, Node* value) {
    if(tokens.find(name) == tokens.end()) {
      tokens[name] = value;
      return true;
    }
    return false;
  }
};






class GotoNode:public Node {
public:
  StringRef target;
  GotoNode():Node(Goto) {
  }
  LabelNode* resolve(ScopeNode* scope) {
    Node* n = scope->resolve(target);
    if(!n) {
      return 0;
    }
    if(n->type == Label) {
      return (LabelNode*)n;
    }
  }
};

class ClassNode:public Node {
public:
  ScopeNode scope;
  StringRef name;
  std::vector<Node*> instructions;
  void resolve() {
    //Resolve alignment and size requirements
    if(!size) {
      size = 1;
    }
    if(!align) {
      align = 1;
    }
  }
  int align; //Required memory alignment for class (or 0 if undefined)
  int size; //Required size for class (excluding padding) (or 0 if undefined)
  ClassNode():Node(Class) {
    
  }
};



class TypeInfo {
public:
  bool isPointer;
  ClassNode* type;
  
};

class Expression:public Node {
public:
  TypeInfo* returnType;
  Expression(const NodeType& type):Node(type) {
    
  }
};


class UnaryNode:public Expression {
public:
  char op;
  Expression* operand;
UnaryNode():Expression(UnaryExpression) {
}
};

class ConstantNode:public Expression {
public:
  ConstantType ctype;
  StringRef value;
  int i32val;
ConstantNode():Expression(Constant) {
}


};


class BinaryExpressionNode:public Expression {
public:
  char op;
  Expression* lhs;
  Expression* rhs;
  bool parenthesized;
  const char* GetFriendlyOpName() {
    switch(op) {
      case '+':
	return "addition";
      case '-':
	return "subtraction";
	case '*':
	return "multiplication";
	case '/':
	return "division";
	case '=':
	  return "assignment";
    }
    return "illegal expression";
  }
  BinaryExpressionNode():Expression(BinaryExpression) {
    
  }
};

class VariableDeclarationNode:public Node {
public:
  StringRef vartype;
  StringRef name;
  BinaryExpressionNode* assignment;
  ClassNode* rclass = 0;
  bool isPointer = false;
  size_t reloffset;
  VariableDeclarationNode():Node(VariableDeclaration) {
  }
};


class VariableReferenceNode:public Expression {
public:
  ScopeNode* scope;
  StringRef id;
  VariableDeclarationNode* variable;
  bool resolve() {
    Node* n = scope->resolve(id);
    if(!n) {
      return false;
    }
    if(n->type != VariableDeclaration) {
      return false;
    }
    variable = (VariableDeclarationNode*)n;
    return variable;
  }
  VariableReferenceNode():Expression(VariableReference) {
    
  }
};




class FunctionNode:public Node {
public:
  bool isExtern;
  StringRef name;
  StringRef returnType;
  ScopeNode scope; //Primary scope of function
  std::vector<VariableDeclarationNode*> args;
  std::vector<Node*> operations;
  FunctionNode(ScopeNode* parent):Node(Function) {
    scope.parent = parent;
  }
};

#endif