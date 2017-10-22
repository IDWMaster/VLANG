#include <stdio.h>
#include "tree.h"
#include <vector>
#include <string>

std::string gencode(Node** nodes, size_t count, ScopeNode* scope);



class VParser:public ParseTree {
public:
  int getRank(char mander) {
    int rank;
    switch(mander) {
	  case '-':
	    rank = 0;
	    break;
      case '+':
	    rank = 1;
	    break;
	  case '*':
	    rank = 2;
	    break;
	  case '/':
	    rank = 3;
	    break;
    }
    return rank;
  }
  template<typename T>
  void swap(T& a, T& b) {
    T tmp = a;
    a = b;
    b = tmp;
  }
  Node* parseExpression(ScopeNode* scope, Node* prev = 0) {
    Node* retval = 0;
    skipWhitespace();
    if(prev) {
      char mander = *ptr;
	ptr++;
	skipWhitespace();
	int rank = 0;
	switch(mander) {
	  case '+':
	  case '-':
	  case '*':
	  case '/':
	  {
	    Node* rhs = parseExpression(scope);
	    if(!rhs) {
	      return 0;
	    }
	    BinaryExpressionNode* bexp = new BinaryExpressionNode();
	    bexp->op = mander;
	    bexp->lhs = prev;
	    bexp->rhs = rhs;
	    if(bexp->rhs->type == BinaryExpression) {
	      BinaryExpressionNode* node = (BinaryExpressionNode*)bexp->rhs;
	      if(getRank(node->op)<getRank(mander) && !node->parenthesized) {
		swap(node->op,bexp->op);
		swap(node->rhs,bexp->lhs);
		swap(node->rhs,node->lhs);
		swap(bexp->lhs,bexp->rhs);
	      }
	    }
	    return bexp;
	  }
	    break;
	}
	
	return 0;
    }else {
    if(isdigit(*ptr)) {
      int oval;
      StringRef erence;
      if(!parseUnsignedInteger(oval,erence)) {
	return 0;
      }
      ConstantNode* node = new ConstantNode();
      node->i32val = oval;
      node->ctype = Integer;
      node->value = erence;
      retval = node;
      
    }else {
      if(isalpha(*ptr)) {
	//Identifier
	StringRef id;
	if(!expectToken(id)) {
	  return 0;
	}
	VariableReferenceNode* varref = new VariableReferenceNode();
	varref->id = id;
	varref->scope = scope;
	retval = varref;
      }else {
	if(*ptr == '(') {
	  ptr++;
	  skipWhitespace();
	  //TODO: Sub-expression.
	  Node* subexp = parseExpression(scope,0);
	  if(subexp->type == BinaryExpression) {
	    ((BinaryExpressionNode*)subexp)->parenthesized = true;
	  }
	  if(!subexp) {
	    return 0;
	  }
	  if(*ptr != ')') {
	    delete subexp;
	    return 0;
	  }
	  ptr++;
	  skipWhitespace();
	  retval = subexp;
	}
      }
    }
    }
    skipWhitespace();
    if(*ptr == ')') {
      return retval;
    }
    if(*ptr == ';') {
      ptr++;
      return retval;
    }else {
      return parseExpression(scope,retval);
    }
  }
  ClassNode* parseClass(ScopeNode* parent) {
    
    skipWhitespace();
    StringRef name;
    int align = 0;
    int size = 0;
    if(!expectToken(name)) {
      return 0;
    }
    skipWhitespace();
    while(*ptr == '.') {
      ptr++;
      StringRef keyword;
      if(!expectToken(keyword)) {
	return 0;
      }
      int wordidx;
      if(!keyword.in(wordidx,"align","size")) {
	return 0;
      }
      skipWhitespace();
      StringRef erence;
      switch(wordidx) {
	case 0:
	{
	  if(!parseUnsignedInteger(align,erence)) {
	    return 0;
	  }
	}
	  break;
	case 1:
	{
	  if(!parseUnsignedInteger(size,erence)) {
	    return 0;
	  }
	}
	  break;
      }
      skipWhitespace();
    }
    
    switch(*ptr) {
      case '{':
      {
	ptr++;
	skipWhitespace();
	if(*ptr == '}') {
	  ptr++;
	  ClassNode* node = new ClassNode();
	  node->align = align;
	  node->name = name;
	  node->size = size;
	  node->scope.parent = parent;
	  if(!parent->add(name,node)) {
	    delete node;
	    return 0;
	  }
	  return node;
	}
      }
	break;
      default:
	return 0;
    }
    
  }
  bool parseUnsignedInteger(int& out, StringRef& seg) {
    skipWhitespace();
    if(!isdigit(*ptr)) {
      return false;
    }
    scan(isdigit,seg);
    
    char* end = (char*)(seg.ptr+seg.count);
    out = strtol(seg.ptr,&end,0);
    return true;
  }
  bool expectToken(StringRef& out) {
    if(!isalnum(*ptr)) {
      return false;
    }
    scan(isalnum,out);
    return true;
  }
  Node* parse(ScopeNode* scope) {
    skipWhitespace();
    char current = *ptr;
    if(isalpha(current)) {
      //Have token
      StringRef token;
      expectToken(token);
      skipWhitespace();
      int keyword;
      std::string cval = token;
      if(token.in(keyword,"class")) {
	switch(keyword) {
	  case 0:
	    return parseClass(scope);
	}
      }else {
	if(isalnum(*ptr)) {
	StringRef token1;
	expectToken(token1);
	skipWhitespace();
	switch(*ptr) {
	  case '=':
	  {
	    ptr++;
	    skipWhitespace();
	    Node* expression = parseExpression(scope);
	    if(expression) {
	      AssignNode* retval = new AssignNode();
	      retval->dest = token1;
	      retval->value = expression;
	      VariableDeclarationNode* vardec = new VariableDeclarationNode();
	      vardec->assignment = retval;
	      vardec->name = token1;
	      vardec->vartype = token;
	      if(!scope->add(token1,vardec)) {
		delete vardec;
		return 0;
	      }
	      return vardec;
	    }
	    
	  }
	    break;
	  case ';':
	  {
	    ptr++;
	    skipWhitespace();
	    VariableDeclarationNode* retval = new VariableDeclarationNode();
	    retval->name = token1;
	    retval->vartype = token;
	    if(!scope->add(token1,retval)) {
		delete retval;
		return 0;
	    }
	    return retval;
	  }
	    break;
	}
	}else {
	  //Parse expression
	  ptr = token.ptr;
	  return parseExpression(scope);
	}
      }
    }
    return 0;
  }
  std::vector<Node*> instructions;
  ScopeNode scope;
  bool error = false;
  VParser(const char* code):ParseTree(code) {
   while(*ptr) {
    Node* instruction = parse(&scope);
    if(instruction) {
    instructions.push_back(instruction);
    }else {
      error = true;
      break;
    }
   }
  }
};

int main(int argc, char** argv) {
  const char* test = "class int .align 4 .size 4 { }\nclass byte .size 1 { }\nclass long .align 8 .size 8 { }\nint x = 5;int y = 2;\nint w = 5*(7*2+6);";
  VParser tounge(test);
  if(!tounge.error) {
    printf("%s\n",gencode(tounge.instructions.data(),tounge.instructions.size(),&tounge.scope).data());
  }else {
    printf("Unexpected end of file\n");
  }
}