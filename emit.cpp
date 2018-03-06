/*
Copyright 2018 Brian Bosak

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/



#include "tree.h"
#include <vector>
#include <sstream>
#include "UVM/emit.h"
#include <string>
#include <map>
#include <list>

//C++ codegen

class PendingFunction {
public:
  std::string name;
  size_t offset; //Offset into UAL bytecode
};
class PendingLabel {
public:
  LabelNode* label;
  size_t offset;
};
class CompilerContext {
public:
  std::vector<Import> ants;
  std::map<StringRef,size_t> functionTable;
  std::list<PendingFunction> pendingFunctionCalls;
  std::list<PendingLabel> pendingLabels;
  std::map<LabelNode*,size_t> labels;
  Assembly* assembler;
  ScopeNode* scope;
  FunctionNode* currentFunction = 0;
  void addExtern(StringRef name, int argcount, int outsize,  bool varargs = false) {
    Import ant;
    ant.argcount = argcount;
    ant.isExternal = true;
    ant.isVarArgs = varargs;
    ant.name = name.ptr;
    ant.namelen = name.count;
    ant.outsize = outsize;
    functionTable[name] = ants.size();
    ants.push_back(ant);
  }
  void addExtern(const char* name, int argcount,int outsize, bool varargs = false) {
    Import ant;
    ant.argcount = argcount;
    ant.isExternal = true;
    ant.isVarArgs = varargs;
    ant.name = name;
    ant.namelen = 0;
    ant.outsize = outsize;
    functionTable[name] = ants.size();
    ants.push_back(ant);
  }
  void add(StringRef name, int argcount, int outsize, bool varargs = false) {
    Import ant;
    ant.argcount = argcount;
    ant.isExternal = false;
    ant.isVarArgs = varargs;
    ant.name = name.ptr;
    ant.namelen = name.count;
    ant.outsize = outsize;
    ant.offset = assembler->len-4;
    functionTable[name] = ants.size();
    ants.push_back(ant);
  }
  void add(LabelNode* label) {
    labels[label] = assembler->len;
  }
  //Generate a linked assembly, to be freed with delete
  void link() {
    size_t oldlen = assembler->len; //Old length
    Assembly code(ants.data(),ants.size());
    code.write(assembler->bytecode+4,assembler->len-4);
    delete[] assembler->bytecode;
    assembler->bytecode = new unsigned char[code.len];
    memcpy(assembler->bytecode,code.bytecode,code.len);
    assembler->len = code.len;
    assembler->capacity = code.len;
    
    int globalOffset = code.len-oldlen; //Global relocation offset
    for(auto pfunc = pendingFunctionCalls.begin();pfunc != pendingFunctionCalls.end();pfunc++) {
      int funcId = (int)functionTable[pfunc->name.data()];
      memcpy(assembler->bytecode+globalOffset+pfunc->offset,&funcId,sizeof(funcId));
    }
    for(auto plabel = pendingLabels.begin();plabel != pendingLabels.end();plabel++) {
      int offset = plabel->offset;
      int realOffset = labels[plabel->label]+globalOffset;
      memcpy(assembler->bytecode+globalOffset+offset,&realOffset,sizeof(realOffset));
    }
  }
  void call(const std::string& mangledName) {
    PendingFunction pfunc;
    pfunc.name = mangledName;
    pfunc.offset = assembler->len+1;
    pendingFunctionCalls.push_back(pfunc);
    assembler->call(0);
  }
  void ret(size_t stacksize) {
    stacksize = -stacksize;
    assembler->getrsp();
    assembler->push(&stacksize,sizeof(stacksize));
    assembler->call(0);
    assembler->setrsp();
    assembler->ret();
  }
  void branch(LabelNode* label) {
    PendingLabel pending;
    pending.label = label;
    int zero = 0;
    assembler->push(&zero,4);
    pending.offset = assembler->len-4;
    assembler->branch();
    
    pendingLabels.push_back(pending);
    
  }
};

void gencode_expression(Expression* expression, CompilerContext& context) {
  switch(expression->type) {
    case BinaryExpression:
    {
      BinaryExpressionNode* bexp = (BinaryExpressionNode*)expression;
      if(!bexp->function) 
      {
	
	gencode_expression(bexp->rhs,context);
	gencode_expression(bexp->lhs,context);
	switch(bexp->op) {
	  case '=':
	  {
	    context.assembler->store();
	  }
	    break;
	}
	
      } else {
	//Convert bexp to function call
	gencode_expression(bexp->function,context);
      }
    }
      break;
	  case UnaryExpression:
	  {
	    UnaryNode* node = (UnaryNode*)expression;
	    if(node->function) {
	      gencode_expression(node->function,context);
	    }else {
	      switch(node->op) {
		case '&':
		{
		  node->operand->isReference = true;
		  gencode_expression(node->operand,context);
		}
		  break;
		case '*':
		{
		  size_t sz = node->operand->returnType->pointerLevels ? sizeof(void*) : node->operand->returnType->type->size;
		  
		  gencode_expression(node->operand,context);
		  if(!node->isReference) {
		  context.assembler->push(&sz,sizeof(void*));
		  context.assembler->load(); 
		  }
		  }
		  break;
	      }
	    }
	  }
	    break;
    case FunctionCall:
    {
      //Evaluate arguments
      FunctionCallNode* call = (FunctionCallNode*)expression;
      size_t argcount = call->args.size();
      Expression** args = call->args.data();
      for(size_t i = 0;i<argcount;i++) {
	gencode_expression(args[argcount-i-1],context);
      }
      if(call->function->function->lambdaCapture) {
	//Lambda captured!
	ClassNode* lambduh = call->function->function->lambdaCapture;
	Node** duh = lambduh->instructions.data();
	size_t len = lambduh->instructions.size();
	for(size_t i = 0;i<len;i++) {
	  switch(duh[i]->type) {
	    case VariableDeclaration:
	    {
	      //Pass memory address of variable to function.
	      VariableDeclarationNode* node = (VariableDeclarationNode*)duh[len-i-1];
	      context.assembler->getrsp();
	      context.assembler->push(&node->lambdaRef->reloffset,sizeof(void*));
	      context.assembler->call(0);
	    }
	      break;
	  }
	}
      }
      //Call function
      FunctionNode* func = call->function->function;
      
      context.call(func->mangle());
    }
      break;
    case Constant:
    {
      ConstantNode* constant = (ConstantNode*)expression;
      switch(constant->ctype) {
	case Boolean:
	{
	  bool ean = constant->i32val;
	  context.assembler->push(&ean,1);
	}
	break;
	case Integer:
	{
	  context.assembler->push(&constant->i32val,4);
	}
	  break;
      }
      if(constant->isReference) {
	context.assembler->vref();
      }
    }
      break;
	case VariableReference:
	{
	  VariableReferenceNode* varref = (VariableReferenceNode*)expression;
	  //Compute memory address of variable
	  context.assembler->getrsp(); //Offset relative to stack pointer
	  context.assembler->push(&varref->variable->reloffset,sizeof(varref->variable->reloffset));
	  context.assembler->call(0);
	  
	  //If variable is a reference, get the memory address of the reference
	  if(varref->variable->isReference) {
	      //Dereference reference
	      size_t size = sizeof(void*);
	      context.assembler->push(&size,sizeof(void*));
	      context.assembler->load();
	    }
	  
	  if(!varref->isReference) {
	    //Read variable
	    
	    
	    size_t size = varref->variable->pointerLevels ? sizeof(void*) : varref->variable->rclass->size;
	    context.assembler->push(&size,sizeof(void*));
	    context.assembler->load();
	    
	  }
	}
	  break;
  }
}
void gencode_function(Node** nodes, size_t count, CompilerContext& context, VariableDeclarationNode** args = 0, size_t arglen = 0);
void gencode_function_header(FunctionNode* func, CompilerContext& context) {
  context.currentFunction = func;
  size_t returnSize = 0;
  if(func->returnType_resolved) {
    returnSize = func->returnType_pointerLevels ? -1 : func->returnType_resolved->type->size;
  }
  if(func->isExtern) {
    context.addExtern(func->mangle().data(),func->args.size() ,returnSize,false); //TODO: Varargs language support
  }else {
    context.add(func->mangle().data(),func->args.size(),returnSize,false); //TODO: Varargs language support
    ScopeNode* prev = context.scope;
    context.scope = &func->scope;
    VariableDeclarationNode** args = func->args.data();
    size_t len = func->args.size();
    gencode_function(func->operations.data(),func->operations.size(),context,args,len);
    context.scope = prev;
  }
  context.currentFunction = 0;
}

static void block_memusage(CompilerContext& context,Node** nodes, size_t count, size_t& memalign, size_t& stacksize) {
  for(size_t i = 0;i<count;i++) {
    switch(nodes[i]->type) {
      case VariableDeclaration:
      {
	VariableDeclarationNode* node = (VariableDeclarationNode*)nodes[i];
	ClassNode* vclass = node->rclass;
	size_t align = (node->pointerLevels+node->isReference) ? sizeof(void*) : vclass->align;
	size_t size= (node->pointerLevels+node->isReference) ? sizeof(void*) : vclass->size;
	node->rclass = vclass;
	if(memalign % align) {
	  if(align % memalign) {
	    memalign = align*memalign;
	  }else {
	    memalign = align;
	  }
	}
	if(stacksize % align) {
	  //Add padding
	  stacksize+=align-(stacksize % align);
	}
	node->reloffset = stacksize;
	stacksize+=size;
      }
	break;
      case IfStatement:
      {
	//Handle case for memory allocation inside if/else blocks
	IfStatementNode* node = (IfStatementNode*)nodes[i];
        block_memusage(context,node->instructions_true.data(),node->instructions_true.size(),memalign,stacksize);
	block_memusage(context,node->instructions_false.data(),node->instructions_false.size(),memalign,stacksize);
	
      }
	break;
      case WhileStatement:
      {
	WhileStatementNode* node = (WhileStatementNode*)nodes[i];
	block_memusage(context,&node->initializer,1,memalign,stacksize);
	block_memusage(context,node->body.data(),node->body.size(),memalign,stacksize);
      }
	break;
    }
  }
}

static void gencode_block(Node** nodes, size_t count, CompilerContext& context) {
  for(size_t i = 0;i<count;i++) {
    switch(nodes[i]->type) {
      case VariableDeclaration:
      {
	VariableDeclarationNode* node = (VariableDeclarationNode*)nodes[i];
	if(node->assignment) {
	  gencode_expression(node->assignment,context);
	}
      }
	break;
      case UnaryExpression:
      case BinaryExpression:
      case FunctionCall:
      {
	gencode_expression((Expression*)nodes[i],context);
      }
	break;
      case IfStatement:
      {
	//Push condition to stack
	IfStatementNode* node = (IfStatementNode*)nodes[i];
	gencode_expression(node->condition,context);
	//Perform branch on condition true -- at this stage, assume that branch will NOT be taken.
	context.branch(&node->jmp_true);
	bool one = true;
	context.assembler->push(&one,1); //Unconditional jump to else clause after branch to true.
	context.branch(&node->jmp_false);
	context.add(&node->jmp_true);
	//If clause
	ScopeNode* prevScope = context.scope;
	context.scope = &node->scope_true;
	gencode_block(node->instructions_true.data(),node->instructions_true.size(),context);
	//Jump past else statement
	context.assembler->push(&one,1);
	context.branch(&node->jmp_end);
	//Else clause (label)
	context.add(&node->jmp_false);
	if(node->instructions_false.size()) {
	  context.scope = &node->scope_false;
	  gencode_block(node->instructions_false.data(),node->instructions_false.size(),context);
	}
	context.scope = prevScope;
	//End of if/else block
	context.add(&node->jmp_end);
      }
	break;
      case WhileStatement:
      {
	WhileStatementNode* node = (WhileStatementNode*)nodes[i];
	if(node->initializer) {
	  Node* rnodes[1];
	  rnodes[0] = node->initializer;
	  gencode_block(rnodes,1,context);
	}
	//Assume that branch will be taken
	context.add(&node->check);
	gencode_expression(node->condition,context);
	context.assembler->call(1);
	context.branch(&node->end); //Exit while loop if condition is false
	//Body of while loop
	context.add(&node->begin);
	ScopeNode* prevScope = context.scope;
	context.scope = &node->scope;
	gencode_block(node->body.data(),node->body.size(),context);
	context.scope = prevScope;
	bool one = true;
	context.assembler->push(&one,1);
	context.branch(&node->check); //Check condition again
	//End of while loop
	context.add(&node->end);
      }
	break;
      case Label:
      {
	context.add((LabelNode*)nodes[i]);
      }
	break;
      case Goto:
      {
	bool one = true;
	context.assembler->push(&one,1);
	context.branch(((GotoNode*)nodes[i])->resolve(context.scope));
      }
	break;
      case ReturnStatement:
      {
	ReturnStatementNode* ret = (ReturnStatementNode*)nodes[i];
	gencode_expression(ret->retval,context);
	context.ret(context.currentFunction->stackSize);
      }
	break;
    }
  }
}


void gencode_function(Node** nodes, size_t count, CompilerContext& context, VariableDeclarationNode** args, size_t arglen) {
  ScopeNode* scope = context.scope;
  Assembly* code = context.assembler;
  size_t memalign = 1;
  size_t stacksize = 0;
  //Phase 0 -- Memory allocation
  if(args) {
    block_memusage(context,(Node**)args,arglen,memalign,stacksize);
  }
  block_memusage(context,nodes,count,memalign,stacksize);
  if(context.currentFunction) {
    if(context.currentFunction->lambdaCapture) {
      //Allocate memory for lambda
      block_memusage(context,context.currentFunction->lambdaCapture->instructions.data(),context.currentFunction->lambdaCapture->instructions.size(),memalign,stacksize);
    }
  }
  
  if(context.currentFunction) {
    context.currentFunction->stackSize = stacksize;
  }
  //Allocate stack
  code->getrsp();
  code->push(&stacksize,sizeof(stacksize));
  code->call(0);
  code->setrsp();
  //Load arguments (if any)
  if(args) {
    for(size_t i = 0;i<arglen;i++) {
      context.assembler->getrsp(); //Compute RSP+offset for each argument
      context.assembler->push(&args[i]->reloffset,sizeof(void*));
      context.assembler->call(0);
      context.assembler->store(); //Store argument into address
    }
  }
  //Load lambda capture values
  if(context.currentFunction) {
    if(context.currentFunction->lambdaCapture) {
      ClassNode* lambduh = context.currentFunction->lambdaCapture;
      Node** duh = lambduh->instructions.data();
      size_t len = lambduh->instructions.size();
      for(size_t i = 0;i<len;i++) {
	switch(duh[i]->type) {
	  case VariableDeclaration:
	  {
	    VariableDeclarationNode* vardec = (VariableDeclarationNode*)duh[i];
	    context.assembler->getrsp();
	    context.assembler->push(&vardec->reloffset,sizeof(void*));
	    context.assembler->call(0);
	    context.assembler->store();
	  }
	    break;
	}
      }
    }
  }
  //Generate code for current function
  gencode_block(nodes,count,context);
  context.ret(stacksize);
  
  //Generate sub-nodes
  for(size_t i = 0;i<count;i++) {
    switch(nodes[i]->type) {
      case Class:
      {
	ClassNode* cls = (ClassNode*)nodes[i];
	//Generate initializer
	if(cls->init->operations.size()) {
	  ScopeNode* prevScope = context.scope;
	  context.scope = &cls->scope;
	  gencode_function(cls->init->operations.data(),cls->init->operations.size(),context);
	  context.scope = prevScope;
	}
	
      }
	break;
      case Function:
      {
	FunctionNode* func = (FunctionNode*)nodes[i];
	gencode_function_header(func,context);
      }
	break;
    }
  }
}




//Generate code (external call)
unsigned char* gencode(Node** nodes, size_t count, ScopeNode* scope, size_t* size) {
  CompilerContext context;
  Assembly code;
  context.addExtern("__uvm_intrinsic_ptradd",2,-1);
  context.addExtern("__uvm_intrinsic_not",1,1);
  context.assembler = &code;
  context.scope = scope;
  gencode_function(nodes,count,context);
  context.link();
  *size = code.len;
  void* rval = malloc(*size);
  memcpy(rval,code.bytecode,code.len);
  return (unsigned char*)rval;
}
