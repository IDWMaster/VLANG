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
class CompilerContext {
public:
  std::vector<Import> ants;
  std::map<StringRef,size_t> functionTable;
  std::list<PendingFunction> pendingFunctionCalls;
  Assembly* assembler;
  ScopeNode* scope;
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
    ant.offset = 0;
    functionTable[name] = ants.size();
    ants.push_back(ant);
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
};

void gencode_expression(Expression* expression, CompilerContext& context) {
  switch(expression->type) {
    case BinaryExpression:
    {
      BinaryExpressionNode* bexp = (BinaryExpressionNode*)expression;
      if(!bexp->function) 
      {
	gencode_expression(bexp->lhs,context);
	gencode_expression(bexp->rhs,context);
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
    case FunctionCall:
    {
      //Evaluate arguments
      FunctionCallNode* call = (FunctionCallNode*)expression;
      size_t argcount = call->args.size();
      Expression** args = call->args.data();
      for(size_t i = 0;i<argcount;i++) {
	gencode_expression(args[i],context);
      }
      //Call function
      FunctionNode* func = call->function->function;
      
      context.call(func->scope.mangle());
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
	  if(!varref->isReference) {
	    //Read variable
	    context.assembler->push(&varref->variable->rclass->size,sizeof(void*));
	    context.assembler->load();
	  }
	}
	  break;
  }
}
void gencode_function(Node** nodes, size_t count, CompilerContext& context);
void gencode_function_header(FunctionNode* func, CompilerContext& context) {
  size_t returnSize = 0;
  if(func->returnType_resolved) {
    returnSize = func->returnType_isPointer ? -1 : func->returnType_resolved->type->size;
  }
  if(func->isExtern) {
    context.addExtern(func->mangle().data(),func->args.size()+(func->thisType ? 1 : 0) ,returnSize,false); //TODO: Varargs language support
  }else {
    context.add(func->mangle().data(),func->args.size()+(func->thisType ? 1 : 0),returnSize,false); //TODO: Varargs language support
    gencode_function(func->operations.data(),func->operations.size(),context);
  }
}

void gencode_function(Node** nodes, size_t count, CompilerContext& context) {
  ScopeNode* scope = context.scope;
  Assembly* code = context.assembler;
  size_t memalign = 1;
  size_t stacksize = 0;
  //Phase 0 -- Memory allocation
  for(size_t i = 0;i<count;i++) {
    switch(nodes[i]->type) {
      case VariableDeclaration:
      {
	VariableDeclarationNode* node = (VariableDeclarationNode*)nodes[i];
	Node* res = scope->resolve(node->vartype);
	if(!res) {
	  return;
	}
	if(res->type != Class) {
	  return;
	}
	ClassNode* vclass = (ClassNode*)res;
	vclass->resolve();
	node->rclass = vclass;
	if(memalign % vclass->align) {
	  if(vclass->align % memalign) {
	    memalign = vclass->align*memalign;
	  }else {
	    memalign = vclass->align;
	  }
	}
	if(stacksize % vclass->align) {
	  //Add padding
	  stacksize+=vclass->align-(stacksize % vclass->align);
	}
	node->reloffset = stacksize;
	stacksize+=vclass->size;
      }
	break;
    }
  }
  //Allocate stack
  code->getrsp();
  code->push(&stacksize,sizeof(stacksize));
  code->call(0);
  code->setrsp();
  
  //Generate code for current function
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
      case FunctionCall:
      {
	gencode_expression((Expression*)nodes[i],context);
      }
	break;
    }
  }
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
  context.assembler = &code;
  context.scope = scope;
  gencode_function(nodes,count,context);
  context.link();
  *size = code.len;
  void* rval = malloc(*size);
  memcpy(rval,code.bytecode,code.len);
  return (unsigned char*)rval;
}