#include "llvm/Support/Host.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
// #include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/raw_ostream.h"

#include "clang/Analysis/CFG.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/ParseAST.h"
#include "clang/AST/ASTContext.h"
#include "clang/Tooling/Tooling.h"

#include <iostream>
#include <vector>

using namespace std;
using namespace clang;
using namespace llvm;

/************************ ast visitor ********************************/
class MyRecursiveASTVisitor
    : public clang::RecursiveASTVisitor<MyRecursiveASTVisitor> {

public:

/************* add code here *****************/
	bool VisitStmt (Stmt *stmt)
	{
		if (this->sourceManager && this->sourceManager->isInMainFile(stmt->getLocStart()))
		{			
			printStatement(stmt);
		}
		return true;
	}


	bool VisitFunctionDecl(FunctionDecl* functionDecl)
	{
		// llvm::outs() << "into here" << "\n";
		// initialize ast context
		this->astContext = &(functionDecl->getASTContext());
		this->sourceManager = &(this->astContext->getSourceManager());
		if(this->sourceManager->isInMainFile(functionDecl->getSourceRange().getBegin()) && functionDecl->hasBody())
		{	
			// traverse statement
			Stmt* functionBody = functionDecl->getBody();
			// VisitStmt(functionBody);
			// for(Stmt::child_iterator iter = functionBody->child_begin(); iter != functionBody->child_end(); iter++)
			// {
			// 	VisitStmt(*iter);	
			// }
			// build cfg
			CFG::BuildOptions bo;
			unique_ptr<CFG> cfg = CFG::buildCFG(functionDecl, functionBody, this->astContext, bo);
			//traverse cfg block from entry
			CFGBlock* cfgEntry = &(cfg->getEntry());
			vector<int> history;
			visitCFGBlock(cfgEntry, history);
		}

		return true;
	}	

 

/************* add code here *****************/

private:

void printStatement(Stmt* stmt)
{
	llvm::outs() << "start location: " << this->sourceManager->getSpellingLineNumber(stmt->getLocStart()) 
		<< " end location: " << this->sourceManager->getSpellingLineNumber(stmt->getLocEnd()) << "\n";
	// stmt->dumpColor();
	llvm::outs() << "stmt type: " << stmt->getStmtClassName() << "\n";
}

void visitCFGBlock(CFGBlock* block, vector<int>& history)
{
	int blockId = block->getBlockID();
	// check history vector
	for(vector<int>::iterator iter = history.begin(); iter != history.end(); iter++)
	{
		if(*iter == blockId)
		{
			return;
		}
	}
	// visit itself
	history.push_back(blockId);
	llvm::outs() << "block id: " << blockId
		<< " block content: ";
	block->dump();
	// llvm::outs() << "\n";

	// visit children
	for(CFGBlock::succ_iterator iter = block->succ_begin(); iter != block->succ_end(); iter++)
	{
		visitCFGBlock(*iter, history);
	}
}

SourceManager* sourceManager;
clang::ASTContext* astContext;
};





/************* add code below *****************/



/*************** add code above ***************/
class MyASTConsumer : public clang::ASTConsumer {
public:
 	explicit MyASTConsumer() : visitor(){}

  	virtual void HandleTranslationUnit(clang::ASTContext &Context) 
  	{
  		// llvm::outs() << "into here translation unit" << "\n";
    	visitor.TraverseDecl(Context.getTranslationUnitDecl());
 	}
private:
  	MyRecursiveASTVisitor visitor;
};

/************************ ast consumer ********************************/
/*class MyASTConsumer
	: public clang::ASTConsumer{
public:
	virtual bool HandleTopLevelDecl(clang::DeclGroupRef d);
};

bool MyASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef d){
	MyRecursiveASTVisitor rv;
	typedef DeclGroupRef::iterator iter;
	for (iter b = d.begin(), e = d.end(); b != e; ++b) {
		rv.TraverseDecl(*b);
	}
	return true; // keep going
}*/

/************************ frontend action ********************************/
class MyASTFrontendAction : public clang::ASTFrontendAction {
public:
	explicit MyASTFrontendAction(){}

	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer( clang::CompilerInstance &Compiler, llvm::StringRef InFile) 
	{
		// llvm::outs() << "into here ast create" << "\n";
	    return std::unique_ptr<clang::ASTConsumer>(new MyASTConsumer());
	}

};

int main(int argc, char **argv) {
	if (argc > 1) 
	{
	    // llvm::outs() << argv[1] << "\n";
	    // llvm::outs() << argv[2] << "\n";
	    clang::tooling::runToolOnCode(new MyASTFrontendAction(), argv[1]);
	}

	return 0;
}

/*int main(int argc, char** argv){
	CompilerInstance ci;
	DiagnosticOptions diagnosticOptions;
	ci.createDiagnostics();	

	// HeaderSearchOptions &headerSearchOptions = ci.getHeaderSearchOpts();

	// headerSearchOptions.AddPath("/usr/include/",
	// 	clang::frontend::Angled, false, false);
	// headerSearchOptions.AddPath("/usr/include/x86_64-linux-gnu/",
	// 	clang::frontend::Angled, false, false);
	// headerSearchOptions.AddPath("/usr/include/linux/",
	// 	clang::frontend::Angled, false, false);

	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
							pto->Triple = llvm::sys::getDefaultTargetTriple();
	llvm::IntrusiveRefCntPtr<TargetInfo> pti(TargetInfo::CreateTargetInfo(ci.getDiagnostics(), pto.getPtr()));
	ci.setTarget(pti.getPtr());

	ci.createFileManager();
	ci.createSourceManager(ci.getFileManager());
	ci.createPreprocessor();
	ci.getPreprocessorOpts().UsePredefines = false;

	MyASTConsumer* astConsumer = new MyASTConsumer();
	ci.setASTConsumer(astConsumer);

	ci.createASTContext();
	const FileEntry *pFile = ci.getFileManager().getFile(argv[1]);

	ci.getSourceManager().createMainFileID(pFile);
	ci.getDiagnosticClient().BeginSourceFile(ci.getLangOpts(),
        	                                 &ci.getPreprocessor());
	clang::ParseAST(ci.getPreprocessor(), astConsumer, ci.getASTContext());
	ci.getDiagnosticClient().EndSourceFile();
}*/