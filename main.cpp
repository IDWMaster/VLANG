#include <stdio.h>
#include "tree.h"
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>

unsigned char* gencode(Node** nodes, size_t count, ScopeNode* scope, size_t* sz);

class ValidationError {
public:
  std::string msg;
  Node* node = 0;
};

class Verifier {
public:
  ScopeNode* rootScope;
  ScopeNode* current;
  std::vector<ValidationError> errors;
  void error(Node* node, const std::string& msg) {
    ValidationError error;
    error.node = node;
    error.msg = msg;
    errors.push_back(error);
    printf("%s\n",msg.data());
  }
  Verifier(ScopeNode* scope):rootScope(scope) {
    current = scope;
  }
  bool validateExpression(Expression* exp) {
    switch(exp->type) {
      case Constant:
      {
	ConstantNode* cnode = (ConstantNode*)exp;
	ClassNode* type = 0;
	bool isptr = false;
	switch(cnode->ctype) {
	  case Character:
	  {
	    type = (ClassNode*)rootScope->resolve("char");
	  }
	    break;
	  case Integer:
	  {
	    type = (ClassNode*)rootScope->resolve("int");
	  }
	    break;
	  case String:
	  {
	    type = (ClassNode*)rootScope->resolve("char");
	    isptr = true;
	  }
	    break;
	}
	cnode->returnType->type = type;
	cnode->returnType->isPointer = isptr;
	if(!type) {
	  error(exp,"Build environment is grinning and holding a spatula.");
	  return false;
	}
      }
	return true;
	  case BinaryExpression:
	  {
	    BinaryExpressionNode* bnode = (BinaryExpressionNode*)exp;
	    if(!validateExpression(bnode->lhs) || !validateExpression(bnode->rhs)) {
	      return false;
	    }
	    TypeInfo* baseinfo = bnode->lhs->returnType;
	    if(bnode->lhs->returnType->isPointer != bnode->rhs->returnType->isPointer) {
	      std::stringstream ss;
	      ss<<"Cannot perform "<<bnode->GetFriendlyOpName()<<" on "<<(std::string)bnode->lhs->returnType->type->name;
	      error(exp,ss.str());
	      return false;
	    }
	    StringRef erence(&bnode->op,1);
	    Node* m = baseinfo->type->scope.resolve(erence);
	    if(m->type != Function) {
	      
	    }
	    
	  }
	  error(exp,"COMPILER BUG: bexp not yet implemented");
	    return false;
    }
    error(exp,"COMPILER BUG: Not yet implemented.");
    return false;
  }
  bool validateNode(Node* node) {
    switch(node->type) {
	case AssignOp: //Illegal opcode (deprecated)
	  return false;
	case BinaryExpression:
	case Constant:
	case UnaryExpression:
	case VariableReference:
	{
	  return validateExpression((Expression*)node);
	}
	  break;
      }
      error(node,"COMPILER BUG: Unsupported node");
      return false;
  }
  bool validate(Node** instructions, size_t count) {
    bool hasValidationErrors;
    for(size_t i = 0;i<count;i++) {
      if(!validateNode(instructions[i])) {
	hasValidationErrors = true;
      }
    }
    return !hasValidationErrors;
  }
};


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
  Expression* parseExpression(ScopeNode* scope, Expression* prev = 0) {
    Expression* retval = 0;
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
	    Expression* rhs = parseExpression(scope);
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
	  case '(':
	  {
	    if(prev->type != VariableReference) {
	      return 0;
	    }
	    FunctionCallNode* retval = new FunctionCallNode();
	    retval->function = (VariableReferenceNode*)prev;
	    while(*ptr && *ptr != ')') {
	      Expression* exp = parseExpression(scope);
	      skipWhitespace();
	      if(*ptr != ')' && *ptr != ',') {
		goto f_fail;
	      }
	      if(*ptr == ',') {
		ptr++;
		skipWhitespace();
	      }
	      if(!exp) {
		goto f_fail;
	      }
	      retval->args.push_back(exp);
	    }
	    
	    if(*ptr == ')') {
	      ptr++;
	      skipWhitespace();
	      return parseExpression(scope,retval);
	    }
	    f_fail:
	    delete retval;
	    return 0;
	  }
	    break;
	  case ';':
	    ptr++;
	    return prev;
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
	  Expression* subexp = parseExpression(scope,0);
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
    
	  ClassNode* node = new ClassNode();
	  node->scope.parent = parent;
    switch(*ptr) {
      case '{':
      {
	ptr++;
	skipWhitespace();
	while(*ptr != '}') {
	  Node* inst = parse(&node->scope);
	  if(!inst) {
	    delete node;
	    return 0;
	  }
	  node->instructions.push_back(inst);
	}
	if(*ptr == '}') {
	  ptr++;
	  node->align = align;
	  node->name = name;
	  node->size = size;
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
    out.ptr = ptr;
    switch(*ptr) {
      case '+':
      case '-':
      case '*':
      case '/':
	out.count = 1;
	ptr++;
	return true;
    }
    if(!isalnum(*ptr)) {
      return false;
    }
    scan(isalnum,out);
    return true;
  }
  GotoNode* parseGoto() {
    skipWhitespace();
    StringRef token;
    if(!expectToken(token)) {
      return 0;
    }
    if(*ptr != ';') {
      return 0;
    }
    ptr++;
    
    GotoNode* node = new GotoNode();
    node->target = token;
    
    return node;
  }
  FunctionNode* parseFunction(ScopeNode* parentScope) {
    FunctionNode* retval = new FunctionNode(parentScope);
    while(*ptr) {
      skipWhitespace();
      StringRef token;
      expectToken(token);
      int keyword;
      if(token.in(keyword,"extern")) {
	switch(keyword) {
	  case 0:
	  {
	    retval->isExtern = true;
	  }
	    break;
	}
      }else {
	//Return type
	retval->returnType = token;
	skipWhitespace();
	//Name
	if(!expectToken(retval->name)) {
	  skipWhitespace();
	  if(*ptr != '(') {
	    delete retval;
	    return 0;
	  }else {
	    //Function with no return type
	    retval->name = retval->returnType;
	    retval->returnType = "";
	  }
	}
	skipWhitespace();
	if(*ptr != '(') {
	  delete retval;
	  return 0;
	}
	ptr++;
	//Argument
	while(*ptr != ')' && *ptr) {
	  VariableDeclarationNode* vardec = new VariableDeclarationNode();
	  if(!expectToken(vardec->vartype)) {
	    goto v_fail;
	  }
	  skipWhitespace();
	  if(!expectToken(vardec->name)) {
	    goto v_fail;
	  }
	  skipWhitespace();
	  continue;
	  v_fail:
	  delete vardec;
	  delete retval;
	  return 0;
	}
	if(!*ptr) {
	  return 0;
	}
	ptr++;
	if(retval->isExtern && *ptr == ';') {
	  ptr++;
	  skipWhitespace();
	  return retval;
	}
      }
    }
    
  }
  int counter = 0;
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
      if(token.in(keyword,"class","goto","extern","alias")) {
	switch(keyword) {
	  case 0:
	    return parseClass(scope);
	  case 1:
	  {
	    return parseGoto();
	  }
	    break;
	  case 2:
	  {
	    ptr = token.ptr;
	    return parseFunction(scope);
	  }
	    break;
	  case 3:
	  {
	    //Alias
	    StringRef aliasName;
	    expectToken(aliasName);
	    skipWhitespace();
	    StringRef aliasValue;
	    expectToken(aliasValue);
	    skipWhitespace();
	    if(*ptr != ';') {
	      return 0;
	    }
	    ptr++;
	    AliasNode* val = new AliasNode();
	    val->dest = aliasValue;
	    if(!scope->add(aliasName,val)) {
	      delete val;
	      return 0;
	    }
	    return val;
	  }
	    break;
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
	    Expression* expression = parseExpression(scope);
	    if(expression) {
	      BinaryExpressionNode* retval = new BinaryExpressionNode();
	      VariableReferenceNode* varref = new VariableReferenceNode();
	      varref->id = token1;
	      varref->scope = scope;
	      retval->lhs = varref;
	      retval->rhs = expression;
	      VariableDeclarationNode* vardec = new VariableDeclarationNode();
	      varref->variable = vardec;
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
	  switch(*ptr) {
	    case ':':
	    {
	      ptr++;
	      LabelNode* rval = new LabelNode();
	      rval->id = counter;
	      counter++;
	      rval->name = token;
	      if(!scope->add(rval->name,rval)) {
		delete rval;
		return 0;
	      }
	      Node* m = scope->resolve(rval->name);
	      return rval;
	    }
	    default:
	    {
		ptr = token.ptr;
		return parseExpression(scope);
	    }
	  }
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
    skipWhitespace();
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
  struct stat us;
  int fd = 0;
  if(argc == 1) {
    fd = open("testprog.vlang",O_RDONLY);
  }else {
    fd = open(argv[1],O_RDONLY);
  }
  fstat(fd,&us);
  char* mander = new char[us.st_size];
  char* ptr = mander;
  while(us.st_size) {
    size_t processed = read(fd,ptr,us.st_size);
    us.st_size-=processed;
    ptr+=processed;
  }
  close(fd);
  
  const char* test = "";
  VParser tounge(mander);
  if(!tounge.error) {
    Verifier place(&tounge.scope);
    if(place.validate(tounge.instructions.data(),tounge.instructions.size())) {
    size_t sz;
    unsigned char* code = gencode(tounge.instructions.data(),tounge.instructions.size(),&tounge.scope,&sz);
    write(STDIN_FILENO,code,sz);
    }else {
      printf("Compilation failed due to validation errors.\n");
    }
  }else {
    printf("Unexpected end of file\n");
  }
}