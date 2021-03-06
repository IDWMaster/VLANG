/*
Copyright 2018 Brian Bosak

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


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
  FunctionNode* currentFunction = 0;
  std::vector<ValidationError> errors;
  
  bool silent = false;
  void error(Node* node, const std::string& msg) {
    if(silent) {
      return;
    }
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
	int isptr = 0;
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
	    isptr = 1;
	  }
	    break;
	  case Boolean:
	  {
	    type = (ClassNode*)rootScope->resolve("bool");
	  }
	    break;
	}
	cnode->returnType = new TypeInfo();
	cnode->returnType->type = type;
	cnode->returnType->pointerLevels = isptr;
	if(!type) {
	  delete cnode->returnType;
	  cnode->returnType = 0;
	  error(exp,"Build environment is grinning and holding a spatula.");
	  return false;
	}
      }
      exp->validated = true;
	return true;
	  case BinaryExpression:
	  {
	    BinaryExpressionNode* bnode = (BinaryExpressionNode*)exp;
	    if(!validateNode(bnode->lhs) || !validateNode(bnode->rhs)) {
	      return false;
	    }
	    TypeInfo* baseinfo = bnode->lhs->returnType;
	    if(bnode->lhs->returnType->pointerLevels != bnode->rhs->returnType->pointerLevels) {
	      std::stringstream ss;
	      ss<<"Cannot perform "<<bnode->GetFriendlyOpName()<<" on "<<(std::string)bnode->lhs->returnType->type->name;
	      error(exp,ss.str());
	      return false;
	    }
	    
	    StringRef erence(&bnode->op,bnode->op2 ? 2 : 1);
	    Node* m = baseinfo->type->scope.resolve(erence);
	    bnode->lhs->isReference = true;
	    if(!m) {
	      if(bnode->op == '=') {
		//Implicit assignment operator
		bnode->function = 0; //No function pointer for implicit operations (UVM instrinsics).
		bnode->validated = true;
		return true;
	      }
	      std::stringstream ss;
	      ss<<"Unable to resolve operator "<<(std::string)erence<<" on "<<(std::string)bnode->lhs->returnType->type->name;
	      error(exp,ss.str());
	      return false;
	    }
	    if(m->type != Function) {
	      error(exp,"COMPILER BUG: Function call overloading not yet supported.");
	      return false;
	    }
	    FunctionNode* f = (FunctionNode*)m;
	    FunctionCallNode* call = new FunctionCallNode();
	    call->args.push_back(bnode->rhs);
	    call->args.push_back(bnode->lhs);
	    VariableReferenceNode* varref = new VariableReferenceNode();
	    varref->function = f;
	    varref->id = f->name;
	    call->function = varref;
	    call->function->scope = &baseinfo->type->scope;
	    validateNode(call);
	    bnode->function = call;
	    bnode->returnType = bnode->function->returnType;
	    bnode->validated = true;
	    return true;
	  }
	      case UnaryExpression:
	      {
		UnaryNode* unode = (UnaryNode*)exp;
		
		unode->operand->isReference = true;
		if(!validateNode(unode->operand)) {
		  return false;
		}
		TypeInfo* baseinfo = unode->operand->returnType;
		StringRef erence(&unode->op,unode->op2 ? 2 : 1);
		
		FunctionCallNode* call = new FunctionCallNode();
		call->args.push_back(unode->operand);
		call->function = new VariableReferenceNode();
		call->function->scope = &baseinfo->type->scope;
		call->function->id = erence;
		silent = true;
		if(!validateNode(call)) {
		  
		unode->operand->isReference = false;
		  delete call;
		  call = 0;
		}
		
		
		silent = false;
		if(!call) {
		  if(unode->op == '&' && unode->operand->type == VariableReference) {
		    unode->function = 0;
		    unode->returnType = new TypeInfo();
		    unode->returnType->pointerLevels = 1;
		    unode->returnType->type = unode->operand->returnType->type;
		    unode->validated = true;
		    return true;
		  }
		  if(unode->op == '*' && unode->operand->returnType->pointerLevels) {
		    //Dereference a pointer
		    unode->function = 0;
		    unode->returnType = new TypeInfo();
		    unode->returnType->pointerLevels = unode->operand->returnType->pointerLevels-1;
		    unode->returnType->type = unode->operand->returnType->type;
		    unode->validated = true;
		    return true;
		  }
		  std::stringstream ss;
		  ss<<"Unable to resolve "<<(std::string)erence<<" on "<<(std::string)unode->operand->returnType->type->name;
		  error(unode,ss.str());
		  return false;
		}
		unode->returnType = call->returnType;
		unode->function = call;
		return true;
		
	      }
		break;
	  case VariableReference:
	  {
	    VariableReferenceNode* varref = (VariableReferenceNode*)exp;
	    if(!varref->resolve()) {
	      std::stringstream ss;
	      ss<<"Unable to resolve "<<(std::string)varref->id;
	      error(varref,ss.str());
	      return false;
	    }
	    if(varref->function) {
	      varref->returnType = varref->function->returnType_resolved;
	      varref->validated = true;
	      return true;
	    }
	    validateNode(varref->variable);
	    TypeInfo* tinfo = new TypeInfo();
	    varref->returnType = tinfo;
	    tinfo->type = varref->variable->rclass;
	    tinfo->pointerLevels = varref->variable->pointerLevels;
	    if(currentFunction != varref->variable->function) {
	      if(!currentFunction->lambdaCapture) {
		currentFunction->lambdaCapture = new ClassNode();
		currentFunction->lambdaCapture->name = "";
	      }
	      ClassNode* lambdaCapture = currentFunction->lambdaCapture;
	      if(lambdaCapture->lambdaRemapTable.find(varref->variable) == lambdaCapture->lambdaRemapTable.end()) {
	      VariableDeclarationNode* vardec = new VariableDeclarationNode();
	      vardec->rclass = varref->variable->rclass;
	      vardec->pointerLevels = varref->variable->pointerLevels;
	      vardec->skipValidateClassName = true; //Don't validate class name against scope in case of conflicts.
	      vardec->function = currentFunction;
	      vardec->isReference = true; //All lambdas capture by reference for now.
	      vardec->lambdaRef = varref->variable;
	      lambdaCapture->instructions.push_back(vardec);
	      lambdaCapture->lambdaRemapTable[varref->variable] = vardec;
	      }
	      varref->variable = lambdaCapture->lambdaRemapTable[varref->variable];
	      
	     // error(varref,"Lambdas not yet supported... Stay tuned!");
	      //return false;
	    }
	    exp->validated = true;
	    return true;
	  }
	    break;
    }
    error(exp,"COMPILER BUG: Unsupported expression type.");
    return false;
  }
  bool validateClass(ClassNode* cls) {
    if(cls->init) {
      delete cls->init;
      
    }
    FunctionNode* init = new FunctionNode(&cls->scope);
    cls->init = init;
    init->isExtern = false;
    init->name = ".init";
    init->operations = cls->instructions;
    init->returnType = "";
    current = &cls->scope;
    
    
    
    
    //Resolve alignment and size requirements
    
    if(!cls->align) {
      cls->align = 1;
    }
    Node** inst = cls->instructions.data();
    size_t len = cls->instructions.size();
    size_t minsize = 0;
    for(size_t i = 0;i<len;i++) {
      switch(inst[i]->type) {
	case VariableDeclaration:
	{
	  VariableDeclarationNode* vdec = (VariableDeclarationNode*)inst[i];
	  size_t size = (vdec->pointerLevels + vdec->isReference) ? sizeof(void*) : vdec->rclass->size;
	  size_t align = ((vdec->pointerLevels + vdec->isReference) ? sizeof(void*) : vdec->rclass->align);
	  minsize+=size;
	  if(cls->align % align) {
	    cls->align*=align;
	  }
	}
	  break;
      }
    }
    cls->size = cls->size>minsize ? cls->size : minsize;
    if(!cls->size) {
      cls->size = 1;
    }
    
    
    return validateNode(init);
    
  }
  
  bool validateFunction(FunctionNode* function) {
    FunctionNode* prev = currentFunction;
    ScopeNode* prevScope = current;
    current = &function->scope;
    currentFunction = function;
    if(function->returnType.count) {
      if(!function->returnType_resolved) {
	ClassNode* n = resolveClass(function,&function->scope,function->returnType);
	if(!n) {
	  currentFunction = prev;
	  return false;
	}
	TypeInfo* tinfo = new TypeInfo();
	tinfo->pointerLevels = function->returnType_pointerLevels;
	tinfo->type = n;
	function->returnType_resolved = tinfo;
	
      }
    }
    VariableDeclarationNode** args = function->args.data();
	size_t argCount = function->args.size();
	for(size_t i = 0;i<argCount;i++) {
	  if(!validateNode(args[i])) {
	    currentFunction = prev;
	    current = prevScope;
	    return false;
	  }
	}
	if(!validate((Node**)function->args.data(),function->args.size())) {
	  currentFunction = prev;
	  current = prevScope;
	  return false;
	}
	Node** funcops = function->operations.data();
	size_t len = function->operations.size();
	for(size_t i = 0;i<len;i++) {
	  switch(funcops[i]->type) {
	    case ReturnStatement:
	    {
	      ((ReturnStatementNode*)funcops[i])->function = function;
	    }
	      break;
	    case VariableDeclaration:
	    {
	      ((VariableDeclarationNode*)funcops[i])->function = function;
	      function->vars.push_back((VariableDeclarationNode*)funcops[i]);
	    }
	      break;
	  }
	}
	bool rval = validate(function->operations.data(),function->operations.size());
	if(function->lambdaCapture) {
	  rval &= validateNode(function->lambdaCapture);
	}
	currentFunction = prev;
	current = prevScope;
	function->validated = rval;
	return rval;
  }
  ClassNode* resolveClass(Node* node,ScopeNode* scope, const StringRef& variable) {
    Node* n = scope->resolve(variable);
    if(!n) {
      goto e_nores;
    }
    if(n->type != Class) {
      goto e_nores;
    }
    return (ClassNode*)n;
    e_nores:
    std::stringstream ss;
    ss<<"Unable to resolve type named "<<(std::string)variable;
    error(node,ss.str());
    return 0;
  }
  bool validateDeclaration(VariableDeclarationNode* varnode) {
    if(!varnode->skipValidateClassName) {
    ClassNode* type = resolveClass(varnode,current,varnode->vartype);
    if(!type) {
      return false;
    }
    varnode->rclass = type;
    }
    if(varnode->assignment && !varnode->isValidatingAssignment) {
      varnode->isValidatingAssignment = true;
      bool rval = validateNode(varnode->assignment);
      varnode->isValidatingAssignment = false;
      varnode->validated = rval;
      return rval;
    }
    varnode->validated = true;
    return true;
  }
  FunctionNode* resolveOverload(FunctionCallNode* call) {
    FunctionNode* func = call->function->function;
    resolve:
    if(!validateNode(func)) {
      return func;
    }
    //Argument counts must match (until we add support for default values)
    if(func->args.size() != call->args.size()) {
      if(!func->nextOverload) {
	return func; //Best overload.
      }
      func = func->nextOverload;
      goto resolve;
    }
    size_t argcount = call->args.size();
    Expression** args = call->args.data();
    VariableDeclarationNode** realArgs = func->args.data();
    for(size_t i = 0;i<argcount;i++) {
      if(!validateNode(args[i])) {
	return func;
      }
      if((realArgs[i]->rclass != args[i]->returnType->type) || (realArgs[i]->pointerLevels != (args[i]->returnType->pointerLevels + args[i]->isReference))) {
	if(!func->nextOverload) {
	  return func;
	}
	func = func->nextOverload;
	goto resolve;
      }
    }
    return func;
  }
  bool validateFunctionCall(FunctionCallNode* call) {
    if(!validateNode(call->function)) {
      return false;
    }
    if(!call->function) {
      std::stringstream ss;
      ss<<(std::string)call->function->id<<" is not a function.";
      error(call,ss.str());
    }
    Expression** args = call->args.data();
    size_t argcount = call->args.size();
    FunctionNode* function = resolveOverload(call);
    if(!validateNode(function)) {
      return false;
    }
    call->function->function = function;
    if(argcount != function->args.size()) {
      std::stringstream ss;
      ss<<"Invalid number of arguments to "<<(std::string)function->name<<". Expected "<<(int)function->args.size()<<", got "<<(int)call->args.size()<<".";
      error(call,ss.str());
      return false;
    }
    for(size_t i = 0;i<argcount;i++) {
      if(!validateNode(args[i])) {
	return false;
      }
    }
    
    VariableDeclarationNode** realArgs = function->args.data();
    for(size_t i = 0;i<argcount;i++) {
      if((realArgs[i]->rclass != args[i]->returnType->type) || (realArgs[i]->pointerLevels != (args[i]->returnType->pointerLevels + args[i]->isReference))) {
	std::stringstream ss;
	ss<<"Invalid argument type. Expected "<<(std::string)realArgs[i]->rclass->name<<", got "<<(std::string)args[i]->returnType->type->name<<".";
	error(call,ss.str());
	return false;
      }
    }
    call->returnType = function->returnType_resolved;
    call->validated = true;
    return true;
    
  }
  bool validateIfStatement(IfStatementNode* node) {
    if(!validateNode(node->condition)) {
      return false;
    }
    size_t count = node->instructions_true.size();
    Node** nodes = node->instructions_true.data();
    for(size_t i = 0;i<count;i++) {
      if(!validateNode(nodes[i])) {
	return false;
      }
    }
    count = node->instructions_false.size();
    nodes = node->instructions_false.data();
    for(size_t i = 0;i<count;i++) {
      if(!validateNode(nodes[i])) {
	return false;
      }
    }
    return true;
  }
  bool validateGoto(GotoNode* dengo) {
    if(!dengo->resolve(current)) {
      std::stringstream ss;
      ss<<"Unable to find "<<(std::string)dengo->target;
      error(dengo,ss.str());
    }
    dengo->validated = true;
    return true;
  }
  bool validateWhileStatement(WhileStatementNode* node) {
    if(!validateNode(node->condition)) {
      return false;
    }
    size_t count = node->body.size();
    Node** nodes = node->body.data();
    for(size_t i = 0;i<count;i++) {
      if(!validateNode(nodes[i])) {
	return false;
      }
    }
    node->validated = true;
    return true;
  }
  bool validateNode(Node* node) {
    if(node->validated) {
      return true;
    }
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
	case Class:
	{
	  return validateClass((ClassNode*)node);
	}
	  break;
	case Function:
	{
	  return validateFunction((FunctionNode*)node);
	}
	  break;
	case VariableDeclaration:
	{
	  return validateDeclaration((VariableDeclarationNode*)node);
	}
	  break;
	case FunctionCall:
	  return validateFunctionCall((FunctionCallNode*)node);
	  break;
	case Alias: //NOP node.
	  node->validated = true;
	  return true;
	case IfStatement:
	  return validateIfStatement((IfStatementNode*)node);
	case Nop:
	case Label:
	{
	  node->validated = true;
	  return true;
	}
	  break;
	case Goto:
	{
	  return validateGoto((GotoNode*)node);
	}
	  break;
	case ReturnStatement:
	{
	  ReturnStatementNode* n = ((ReturnStatementNode*)node);
	  if(!n->function) {
	    error(n,"Cannot return outside of a function.");
	    return false;
	  }
	  
	  if(!validateExpression(n->retval)) {
	    return false;
	  }
	  if((n->retval->returnType->pointerLevels != n->function->returnType_pointerLevels) || (n->retval->returnType->type != n->function->returnType_resolved->type)) {
	    return false;
	  }
	  n->validated = true;
	  return true;
	}
	case WhileStatement:
	  return validateWhileStatement((WhileStatementNode*)node);
      }
      error(node,"COMPILER BUG: Unsupported node");
      return false;
  }
  bool validate(Node** instructions, size_t count) {
    bool hasValidationErrors;
    for(size_t i = 0;i<count;i++) {
      if(!validateNode(instructions[i])) {
	return false;
      }
    }
    return true;
  }
};


class VParser:public ParseTree {
public:
  int getRank(char mander) {
    int rank;
    switch(mander) {
      case '=':
	rank = -2;
	break;
      case '>':
      case '<':
	rank = -1;
	break;
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
  int vParens = 0; //Virtual parenthesis levels
  Expression* parseExpression(ScopeNode* scope, Expression* prev = 0) {
    Expression* retval = 0;
    skipWhitespace();
    if(prev) {
      char mander = *ptr;
	ptr++;
	skipWhitespace();
	int rank = 0;
	char op2 = 0;
	switch(mander) {
	  case '<':
	  case '>':
	  case '+':
	  case '-':
	  case '*':
	  case '/':
	  case '=':
	  {
	    short word = (short)mander | (((short)*ptr) << 8);
	    switch(word) {
	      case 15678: //>=
	      case 15676: //<=
	      case 11051: //++
	      case 11565: //--
	      case 15659: //+=
	      case 15661: //-=
	      {
		op2 = *ptr;
		ptr++;
	      }
		break;
	    }
	    switch(word) {
	      case 11051:
	      case 11565:
	      {
		UnaryNode* unode = new UnaryNode();
		unode->op = mander;
		unode->op2 = op2;
		unode->operand = prev;
		return unode;
	      }
	    }
	    Expression* rhs = parseExpression(scope);
	    if(!rhs) {
	      return 0;
	    }
	    BinaryExpressionNode* bexp = new BinaryExpressionNode();
	    bexp->op = mander;
	    bexp->op2 = op2;
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
	skipWhitespace();
	int match;
	if(id.in(match,"false","true")) {
	  switch(match) {
	    case 0:
	    case 1:
	    {
	      ConstantNode* tine = new ConstantNode();
	      tine->ctype = Boolean;
	      tine->i32val = match;
	      retval = tine;
	    }
	  }
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
	}else {
	  //Various unary operations
	  switch(*ptr) {
	    case '*':
	    case '&':
	    {
	      
	      char op = *ptr;
	      ptr++;  
	      short word = (short)op | (((short)*ptr) << 8);
	      
	      skipWhitespace();
	      //Memory address of expression
	      vParens++;
	      Expression* rhs = parseExpression(scope);
	      vParens--;
	      if(!rhs) {
		return 0;
	      }
	      UnaryNode* unode = new UnaryNode();
	      unode->op = op;
	      unode->operand = rhs;
	      retval = unode;
	    }
	      break;
	  }
	}
      }
    }
    }
    skipWhitespace();
    if(vParens) {
	return retval;
    }
    if(*ptr == ')' || *ptr == ',') {
      return retval;
    }
    if(*ptr == ';') {
      ptr++;
      return retval;
    }else {
      
      if(!retval) {
	return 0;
      }
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
	  node->scope.name = name;
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
	  switch(inst->type) {
	    case Function:
	    {
	      FunctionNode* func = (FunctionNode*)inst;
	      func->thisType = node;
	      VariableDeclarationNode* vardec = new VariableDeclarationNode();
	      vardec->assignment = 0;
	      vardec->pointerLevels = 1;
	      vardec->name = "this";
	      vardec->vartype = name;
	      vardec->rclass = node;
	      vardec->function = func;
	      func->scope.add(vardec->name,vardec);
	      func->args.push_back(vardec);
	    }
	      break;
	  }
	  node->instructions.push_back(inst);
	  
	skipWhitespace();
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
      case '=':
      case '>':
      case '<':
	out.count = 1;
	char op = *ptr;
	ptr++;
	short word = ((short)op) | (((short)*ptr) << 8);
	switch(word) {
	  case 15678: //>=
	      case 15676: //<=
	      case 11051: //++
	      case 11565: //--
	      case 15659: //+=
	      case 15661: //-=
		ptr++;
		out.count = 2;
		break;
	}
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
	retval->scope.name = retval->name;
	ptr++;
	//Argument
	while(*ptr != ')' && *ptr) {
	  VariableDeclarationNode* vardec = new VariableDeclarationNode();
	  vardec->function = retval;
	  if(!expectToken(vardec->vartype)) {
	    goto v_fail;
	  }
	  skipWhitespace();
	  if(!expectToken(vardec->name)) {
	    goto v_fail;
	  }
	  skipWhitespace();
	  if(*ptr == ',') {
	    ptr++;
	    skipWhitespace();
	  }
	  if(!retval->scope.add(vardec->name,vardec)) {
	    goto v_fail;
	  }
	  retval->args.push_back(vardec);
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
	  if(!scope.add(retval->name,retval)) {
	    FunctionNode* onode = (FunctionNode*)scope.resolve(retval->name);
	    //Add overload
	    retval->nextOverload = onode->nextOverload;
	    onode->nextOverload = retval;
	  }
	  return retval;
	}
	skipWhitespace();
	if(*ptr != '{') 
	{
	  return 0;
	}
	ptr++;
	skipWhitespace();
	while(*ptr != '}') {
	  if(!(*ptr)) {
	    goto l_free;
	  }
	  Node* node = parse(&retval->scope);
	  if(node) {
	    retval->operations.push_back(node);
	  }else {
	    if(*ptr != '}') {
	      goto l_free;
	    }
	  }
	}
	ptr++;
	if(!scope.add(retval->name,retval)) {
	    FunctionNode* onode = (FunctionNode*)scope.resolve(retval->name);
	    //Add overload
	    retval->nextOverload = onode->nextOverload;
	    onode->nextOverload = retval;
	  }
	return retval;
	l_free:
	if(retval) {
	  delete retval;
	  return 0;
	}
      }
    }
    
  }
  
  bool parseTypeName(StringRef& type, int& ptrlevels) {
    
    if(!expectToken(type)) {
      return false;
    }
    while(*ptr == '*') {
      ptrlevels++;
      ptr++;
    }
    return true;
  }
  
  int counter = 0;
  Node* parse(ScopeNode* scope) {
    skipWhitespace();
    char current = *ptr;
    if(current == ';') {
      ptr++;
      return new Nope();
    }
    //Check if function
    StringRef funcname;
    if(expectToken(funcname)) {
      skipWhitespace();
      ptr = funcname.ptr;
      FunctionNode* node = parseFunction(scope);
      if(node) {
	return node;
      }
    }
    ptr = funcname.ptr;
    if(isalpha(current)) {
      //Have token
      StringRef token;
      int ptrLevels = 0;
      parseTypeName(token,ptrLevels);
      skipWhitespace();
      int keyword;
      std::string cval = token;
      if(token.in(keyword,"class","goto","extern","alias","if","while","for","return")) {
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
	  case 4:
	  {
	    IfStatementNode* conditional = new IfStatementNode();
	      conditional->scope_false.parent = scope;
	      conditional->scope_true.parent = scope;
	      if(*ptr != '(') {
		goto err_condition;
	      }
	      ptr++;
	      skipWhitespace();
	      conditional->condition = parseExpression(scope);
	      if(!conditional->condition) {
		goto err_condition;
	      }
	      skipWhitespace();
	      if(*ptr != ')') {
		goto err_condition;
	      }
	      ptr++;
	      skipWhitespace();
	      if(*ptr != '{') {
		goto err_condition;
	      }
	      ptr++;
	      skipWhitespace();
	      while(*ptr) {
		if(*ptr == '}') {
		  ptr++;
		  skipWhitespace();
		  StringRef token;
		  if(expectToken(token)) {
		    int id;
		    if(!token.in(id,"else")) {
		      ptr = token.ptr;
		      return conditional;
		    }else {
		      //Parse else block
		      skipWhitespace();
		      if(*ptr != '{') {
			goto err_condition;
		      }
		      ptr++;
		      while(*ptr) {
			if(*ptr == '}') {
			  ptr++;
			  skipWhitespace();
			  return conditional;
			}
			Node* node = parse(&conditional->scope_false);
			if(node) {
			  conditional->instructions_false.push_back(node);
			}else {
			  if(*ptr != '}') {
			    goto err_condition;
			  }
			}
		      }
		    }
		  }else {
		    return conditional;
		  }
		}else {
		  Node* node = parse(&conditional->scope_true);
			if(node) {
			  conditional->instructions_true.push_back(node);
			}else {
			  if(*ptr != '}') {
			    goto err_condition;
			  }
			}
		}
	      }
	      err_condition:
	      if(conditional->condition) {
		delete conditional->condition;
	      }
	      delete conditional;
	      return 0;
	  }
	    break;
	    case 5:
	    {
	      //While statement
	      WhileStatementNode* retval = new WhileStatementNode();
	      retval->scope.parent = scope;
	      skipWhitespace();
	      if(*ptr != '(') {
		goto while_fail;
	      }
	      ptr++;
	      retval->condition = parseExpression(scope,0);
	      skipWhitespace();
	      if(*ptr != ')') {
		goto while_fail;
	      }
	      ptr++;
	      skipWhitespace();
	      if(!retval->condition) {
		goto while_fail;
	      }
	      if(*ptr != '{') {
		goto while_fail;
	      }
	      ptr++;
	      while(*ptr) {
		skipWhitespace();
		Node* node = parse(&retval->scope);
		if(!node && *ptr != '}') {
		  goto while_fail;
		}
		if(node) {
		  retval->body.push_back(node);
		}
		skipWhitespace();
		if(*ptr == '}') {
		  ptr++;
		  return retval;
		}
		
		
	      }
	      while_fail:
	      delete retval;
	      return 0;
	    }
	      break;
	    case 6:
	    {
	      //While statement
	      WhileStatementNode* retval = new WhileStatementNode();
	      retval->scope.parent = scope;
	      skipWhitespace();
	      Node* incrementor = 0;
	      if(*ptr != '(') {
		goto for_fail;
	      }
	      ptr++;
	      retval->initializer = parse(&retval->scope); //Initializer exists in scope of for loop body (inaccessible outside of for loop)
	      
	      skipWhitespace();
	      if(*ptr != ';' && !retval->initializer) {
		goto for_fail;
	      }
	      if(*ptr == ';') {
	      ptr++;
	      }
	      skipWhitespace();
	      retval->condition = parseExpression(&retval->scope,0);
	      skipWhitespace();
	      if(*ptr != ';' && !retval->condition) {
		goto for_fail;
	      }
	      if(!retval->condition) {
		ConstantNode* cnode = new ConstantNode();
		cnode->ctype = Boolean;
		cnode->i32val = 1;
		cnode->isReference = false;
		retval->condition = cnode;
	      }
	      if(*ptr == ';') {
		ptr++;
	      }
	      skipWhitespace();
	      incrementor = parse(&retval->scope);
	      skipWhitespace();
	      if(*ptr != ')') {
		goto for_fail;
	      }
	      ptr++;
	      skipWhitespace();
	      
	      if(*ptr != '{') {
		goto for_fail;
	      }
	      ptr++;
	      while(*ptr) {
		skipWhitespace();
		Node* node = parse(&retval->scope);
		if(!node && *ptr != '}') {
		  goto for_fail;
		}
		if(node) {
		  retval->body.push_back(node);
		}
		skipWhitespace();
		if(*ptr == '}') {
		  ptr++;
		  if(incrementor) {
		    retval->body.push_back(incrementor);
		  }
		  return retval;
		}
		
		
	      }
	      for_fail:
		if(incrementor) {
		  delete incrementor;
		}
	      delete retval;
	      return 0;
	    }
	      break;
	      case 7:
	      {
		Expression* rval = parseExpression(scope);
		if(!rval) {
		  return 0;
		}
		ReturnStatementNode* rnode = new ReturnStatementNode();
		rnode->retval = rval;
		return rnode;
	      }
		break;
	}
      }else {
	if(isalnum(*ptr)) {
	StringRef token1;
	expectToken(token1);
	skipWhitespace();
	const char* m_ptr = ptr;
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
	      vardec->pointerLevels = ptrLevels;
	      varref->variable = vardec;
	      vardec->assignment = retval;
	      vardec->name = token1;
	      vardec->vartype = token;
	      if(!scope->add(token1,vardec)) {
		delete vardec;
		return 0;
	      }
	      retval->op = '=';
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
	    retval->pointerLevels = ptrLevels;
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
    return parseExpression(scope);
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
  const char* filename = "testprog.vlang";
  if(argc == 1) {
    fd = open(filename,O_RDONLY);
  }else {
    filename = argv[1];
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
  tounge.scope.name = "global";
  if(!tounge.error) {
    Verifier place(&tounge.scope);
    if(place.validate(tounge.instructions.data(),tounge.instructions.size())) {
    size_t sz;
    unsigned char* code = gencode(tounge.instructions.data(),tounge.instructions.size(),&tounge.scope,&sz);
    write(STDOUT_FILENO,code,sz);
    }else {
      printf("Compilation failed due to validation errors.\n");
    }
  }else {
    printf("Unexpected end of file\n");
  }
}
