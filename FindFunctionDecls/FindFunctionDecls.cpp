#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class FindLocatedFunctionVisitor : public RecursiveASTVisitor<FindLocatedFunctionVisitor>
{
public:
	explicit FindLocatedFunctionVisitor(ASTContext* Context, unsigned int LineNum) : Context(Context), LineNumber(LineNum){
		 SrcManager = &(Context->getSourceManager());
	}

	// bool VisitCXXRecordDecl(CXXRecordDecl *Declaration)
	// {
	// 	 FullSourceLoc FullLocation = Context->getFullLoc(Declaration->getLocStart());
	// 	 if (FullLocation.isValid())
	// 	 {
	// 	        llvm::outs() << "Find declaration : "<< Declaration->getQualifiedNameAsString() << " at "
	// 	                     << FullLocation.getSpellingLineNumber() << ":"
	// 	 }
	// 	 return true;
	// }

	bool VisitFunctionDecl(FunctionDecl *Declaration) 
	{ 
		// if (Declaration->isThisDeclarationADefinition() ) 
		if(Declaration->isDefined() )
		{ 

		      FullSourceLoc StartLocation = Context->getFullLoc(Declaration->getLocStart());
		      FullSourceLoc EndLocation = Context->getFullLoc(Declaration->getLocEnd());
		      if (StartLocation.isValid()  && SrcManager->isInMainFile(Declaration->getSourceRange().getBegin())&& EndLocation.isValid())
		      {
			      	// llvm::outs() << "@" <<Declaration->getQualifiedNameAsString() << "@" << StartLocation.getSpellingLineNumber() <<  "@"
			       //               << EndLocation.getSpellingLineNumber() << "\n" ;
				if(StartLocation.getSpellingLineNumber()  <= LineNumber &&  EndLocation.getSpellingLineNumber() >= LineNumber)
				{
					// llvm::outs() << "function : "<< Declaration->getNameInfo().getName().getAsString()<< " start at "
				 //                     << StartLocation.getSpellingLineNumber() << ";" <<  " end ait "
				 //                     << EndLocation.getSpellingLineNumber() <<  " ;include line " << LineNumber << "\n";
					// llvm::outs() << " function parameters : ";
				 //             for(unsigned int i = 0; i < Declaration->getNumParams(); i++)
					// {
					// 	llvm::outs() << " " << Declaration->parameters()[i]->getQualifiedNameAsString();
					// }
					// llvm::outs() << " \n function body : ";
				 //             Declaration->getBody()->printPretty(llvm::outs(), nullptr, PrintingPolicy(Context->getLangOpts()));
					// ///function body
				 //             Declaration->getBody()->printPretty(llvm::outs(), nullptr, PrintingPolicy(Context->getLangOpts())) ;
					// ///function name
					// llvm::outs() << "@" << Declaration-> getQualifiedNameAsString()<< "@";
					// //function args
				 //             for(unsigned int i = 0; i < Declaration->getNumParams(); i++)
					// {
					// 	llvm::outs()  << Declaration->parameters()[i]->getQualifiedNameAsString() <<  ",";
					// }
					//line number info
				             llvm::outs()   << "@" << StartLocation.getSpellingLineNumber() <<  "@"
				                     << EndLocation.getSpellingLineNumber() <<   "@"  << LineNumber;
				}
		    }
		}

    		return true; 
 	}

private:
	SourceManager* SrcManager;
	ASTContext *Context;
	unsigned int LineNumber;
};


class FindLocatedFunctionConsumer : public clang::ASTConsumer {
public:
 	explicit FindLocatedFunctionConsumer(ASTContext *Context,unsigned int LineNum) : Visitor(Context, LineNum) {}

  	virtual void HandleTranslationUnit(clang::ASTContext &Context) 
  	{
    		Visitor.TraverseDecl(Context.getTranslationUnitDecl());
 	}
private:
  	FindLocatedFunctionVisitor Visitor;
};

class FindLocatedFunctionAction : public clang::ASTFrontendAction {
public:
	explicit FindLocatedFunctionAction(unsigned int LineNum) : LineNumber(LineNum) {}

	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer( clang::CompilerInstance &Compiler, llvm::StringRef InFile) 
	{
	    	return std::unique_ptr<clang::ASTConsumer>( new FindLocatedFunctionConsumer(&Compiler.getASTContext(), LineNumber));
	}

private:
	///location of interested code
	unsigned int LineNumber;
};

int main(int argc, char **argv) {
	if (argc > 2) 
	{
	    // llvm::outs() << argv[1] << "\n";
	    // llvm::outs() << argv[2] << "\n";
	    clang::tooling::runToolOnCode(new FindLocatedFunctionAction(atoi(argv[1])), argv[2]);
	}
}