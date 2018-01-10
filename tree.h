#ifndef TREE_HEADER
#define TREE_HEADER
#include "libparse/parser.h"
#include <map>
#include <sstream>
#include <vector>


using namespace libparse;


enum NodeType {
  Class, Scope, VariableDeclaration, AssignOp, Constant, BinaryExpression, VariableReference, Goto, Label, UnaryExpression, Function, Alias, FunctionCall, IfStatement, WhileStatement, ReturnStatement
};
enum ConstantType {
  Integer, String, Character, Boolean
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
  LabelNode():Node(Label) {
  }
};


class AliasNode:public Node {
public:
  StringRef dest;
  AliasNode():Node(Alias) {
  }
};

class ScopeNode:public Node {
public:
  ScopeNode* parent;
  std::map<StringRef,Node*> tokens;
  StringRef name; //Optional name of scope
  std::string mangled_name;
  void __mangle(std::stringstream& ss) {
    if(parent) {
      parent->__mangle(ss);
    }
    if(name.count == 0) {
      ss<<".";
    }else {
      ss<<(std::string)name<<"\\";
    }
  }
  std::string& mangle() {
    if(mangled_name.size()) {
      return mangled_name;
    }
    std::stringstream ss;
    __mangle(ss);
    mangled_name = ss.str();
    return mangled_name;
  }
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
      Node* rval = tokens[name];
      if(rval->type == Alias) {
	return resolve(((AliasNode*)rval)->dest);
      }
      return rval;
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

class FunctionNode;
class ClassNode:public Node {
public:
  ScopeNode scope;
  StringRef name;
  std::vector<Node*> instructions;
  FunctionNode* init = 0;
  
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
  size_t size; //Required size for class (excluding padding) (or 0 if undefined)
  ClassNode():Node(Class) {
    
  }
};



class TypeInfo {
public:
  int pointerLevels;
  ClassNode* type;
  
};

class Expression:public Node {
public:
  bool isReference = false;
  TypeInfo* returnType;
  Expression(const NodeType& type):Node(type) {
    
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

class BinaryExpressionNode;
class VariableDeclarationNode:public Node {
public:
  StringRef vartype;
  StringRef name;
  BinaryExpressionNode* assignment;
  bool isValidatingAssignment = false;
  ClassNode* rclass = 0;
  int pointerLevels = 0;
  size_t reloffset;
  VariableDeclarationNode():Node(VariableDeclaration) {
  }
};


class VariableReferenceNode:public Expression {
public:
  ScopeNode* scope;
  StringRef id;
  VariableDeclarationNode* variable = 0;
  FunctionNode* function = 0;
  bool resolve() {
    Node* n = scope->resolve(id);
    if(!n) {
      return false;
    }
    if(n->type != VariableDeclaration) {
      if(n->type == Function) {
	function = (FunctionNode*)n;
	return true;
      }
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
  size_t stackSize;
  bool isExtern;
  StringRef name;
  StringRef returnType;
  bool returnType_pointerLevels = false;
  TypeInfo* returnType_resolved = 0;
  ScopeNode scope; //Primary scope of function
  std::vector<VariableDeclarationNode*> args;
  std::vector<Node*> operations;
  ClassNode* thisType = 0; //Type of "this" pointer, if applicable (must be passed as last argument to function if nonzero).
  FunctionNode* nextOverload = 0;
  std::string mangled_name;
  std::string& mangle() {
    if(!mangled_name.size()) {
    std::stringstream ss;
    ss<<scope.mangle()<<"(";
    size_t arglen;
    VariableDeclarationNode** args;
    args = this->args.data();
    arglen = this->args.size();
    for(size_t i = 0;i<arglen;i++) {
      ss<<(std::string)args[i]->rclass->scope.mangle();
      for(size_t c = 0;c<args[i]->pointerLevels;c++ /*This is how C++ was invented*/) {
	ss<<'*';
      }
      ss<<"\\";
    }
    ss<<")";
    if(returnType_resolved) {
      ss<<returnType_resolved->type->scope.mangle();
    }
    mangled_name = ss.str();
    }
    return mangled_name;
  }
  FunctionNode(ScopeNode* parent):Node(Function) {
    scope.parent = parent;
  }
};




class FunctionCallNode:public Expression {
public:
  VariableReferenceNode* function;
  std::vector<Expression*> args;
  FunctionCallNode():Expression(FunctionCall) {
    
  }
};


class BinaryExpressionNode:public Expression {
public:
  char op;
  char op2 = 0; //Second byte of op
  Expression* lhs;
  Expression* rhs;
  bool parenthesized;
  FunctionCallNode* function;
  const char* GetFriendlyOpName() {
    short op = this->op;
    if(op2) {
      op = op | (op2 << 8);
    }
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
	case 15678:
	  return "greater than or equal to";
	case 15676:
	  return "less than or equal to";
	case 11051:
	case 15659:
	return "increment";
	case 11565:
	case 15661:
	  return "decrement";
    }
    return "illegal expression";
  }
  BinaryExpressionNode():Expression(BinaryExpression) {
    
  }
};


class IfStatementNode:public Node {
public:
  std::vector<Node*> instructions_true; //If branch
  std::vector<Node*> instructions_false; //Else branch
  ScopeNode scope_true;
  ScopeNode scope_false;
  LabelNode jmp_true;
  LabelNode jmp_false;
  LabelNode jmp_end;
  Expression* condition;
IfStatementNode():Node(IfStatement) {
}
};

class WhileStatementNode:public Node {
public:
WhileStatementNode():Node(WhileStatement) {
}
  Expression* condition;
  ScopeNode scope;
  Node* initializer;
  std::vector<Node*> body;
  LabelNode check;
  LabelNode begin; //Beginning of loop
  LabelNode end; //End of loop
};


class UnaryNode:public Expression {
public:
  char op;
  char op2 = 0;
  Expression* operand;
  FunctionCallNode* function;
UnaryNode():Expression(UnaryExpression) {
}
};


class ReturnStatementNode:public Node {
public:
  Expression* retval;
  FunctionNode* function = 0;
  ReturnStatementNode():Node(ReturnStatement) {
  }
  
};

#endif