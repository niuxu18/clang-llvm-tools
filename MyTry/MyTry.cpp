#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/SmallString.h"
using namespace llvm;

namespace {

	cl::list<std::string> LogLoc("loc", cl::desc("log location"), cl::OneOrMore);


	struct MyTryPass : public FunctionPass {
		static char ID;
		DependenceAnalysis* dependenceAnalysis;
		DominatorTree* dominatorTree;
		Function* currFunction;
		unsigned logLoc;// from 1
		unsigned controlLoc;// from 1
		const StringRef RET = "_ret";
		typedef SmallVector<Instruction*, 16> InstVector;
		typedef SmallVector<User*, 16> UserVector;
		typedef SmallString<50> MySmallString;
		typedef SmallVector<MySmallString, 16> StringVector;


		MyTryPass() : FunctionPass(ID)
		{
			for(auto &loc : LogLoc)
			{
				logLoc = std::stoi(loc);
				errs() << logLoc << "\n";
			}
		}

//		job done when initialization
		virtual bool doInitialization(Module &) override
		{
			dependenceAnalysis = nullptr;
			dominatorTree = nullptr;
			currFunction = nullptr;
			controlLoc = 0;
			return false;
		}

//		job done when finalization
		virtual bool doFinalization(Module &)
		{
			return false;
		}

		bool runOnFunction(Function &function) override
		{
			// get dependence analysis pass
			dependenceAnalysis = &getAnalysis<DependenceAnalysis>();
			dominatorTree = &(getAnalysis<DominatorTreeWrapperPass>().getDomTree());

			// function info (name)
			// errs() << "now function is: " << function.getName() << "\n";
			currFunction = &function;
			Instruction* logInst = getLogInstForLogLoc();
			if(logInst != nullptr)
			{
//				logInst->dump();
				getControlFunctionForLogInst(logInst);
				getDataFunctionForLogInst(logInst);
			}

			// do not modify function
			return false;
		}

//		override to get prerequisite passes
		void getAnalysisUsage(AnalysisUsage &analysisUsage) const override
		{
			// memory dependence
			analysisUsage.addRequired<DependenceAnalysis>();
			// dominator tree
			analysisUsage.addRequired<DominatorTreeWrapperPass>();
			analysisUsage.setPreservesAll();
		}

//		get log call statement(the first call instruction when reverse)
		Instruction* getLogInstForLogLoc()
		{
			Instruction* logInst;
			// reverse
			for(inst_iterator instIter = --inst_end(currFunction); instIter != inst_begin(currFunction); instIter--)
			{
				logInst = &*instIter;
				unsigned loc = getDebugLocation(logInst);
				if(loc == logLoc)
				{
					CallInst* callInst = dyn_cast<CallInst>(logInst);
					if(callInst)
					{
						return logInst;
					}// end of call verify
				}// end of location verify
			}// end of reverse

			return nullptr;
		}// end of getLogCallInstForLogLoc

//		get data dependence function info for given log instruction
		void getDataFunctionForLogInst(Instruction* logInst)
		{
			// get sub instructions for log
			InstVector insts;
			getInstsForLogInst(logInst, insts);
			// get function info for sub instruction
			StringVector functions;
			getFunctionInfoForInsts(insts, functions);
			// print or write out function info
			errs() << "@ddg@: " << "\n";
			printFunctionInfo(functions);
		}


//		get control dependence info(function info) for given log instruction
		void getControlFunctionForLogInst(Instruction* logInst)
		{
			// log basic block
			BasicBlock* logBB = logInst->getParent();
			// get control dependence function info
			BasicBlock* controlBB;
			controlLoc = getControlLocForLogBasicBlock(logBB, controlBB);
			if(controlLoc && controlBB)
			{
				// controlBB->dump();
				errs() << "@cdg@: " << controlLoc << "\n";
				getFunctionInfoForControlLoc(controlBB);
			}
		}


//		get instruction vector for given instruction
		void getInstsForLogInst(Instruction* logInst, InstVector& insts)
		{
			// find all arguments for log instruction
			if(isa<CallInst>(logInst))
			{
				CallInst* callInst = dyn_cast<CallInst>(logInst);
				unsigned numArgs = callInst->getNumArgOperands();
				for(unsigned i = 0; i < numArgs; i++)
				{
					Instruction* argInst = dyn_cast<Instruction>(callInst->getArgOperand(i));
					if(!argInst)
					{
						continue;
					}
					insts.push_back(argInst);
				}// end of args
			}

		}// end of getInstsForLogInst

//		control dependence: immediate dominator which is unique predecessor
		unsigned getControlLocForLogBasicBlock(BasicBlock* logBB, BasicBlock*& controlBB)
		{
			// dst basic block dominates[not same] src basic block
			auto idom = dominatorTree->getNode(logBB)->getIDom();
			if(idom)
			{
				BasicBlock* dstBB = dominatorTree->getNode(logBB)->getIDom()->getBlock();
				if(dstBB)
				{
					// terminator instruction
					TerminatorInst* terminatorInst = dstBB->getTerminator();
					// unique predecessor
					if(logBB->getUniquePredecessor() == dstBB)
					{
						controlBB = dstBB;
						//controlBB->dump();
						return getDebugLocation(terminatorInst);
					}
					else
					{
						// find control location for dominator
						return getControlLocForLogBasicBlock(dstBB, controlBB);
					}
				}
			}
			// no control dependence
			controlBB = nullptr;
			return 0;
		}

//		get function info for control location(data dependence and so on)
		void getFunctionInfoForControlLoc(BasicBlock* controlBB)
		{
			// fetch all instructions
			InstVector insts;
			getInstsForControlLoc(controlBB, insts);
			// deal with instructions
			StringVector functions;
			getFunctionInfoForInsts(insts, functions);
			// deal with functions(print out or write out)
			printFunctionInfo(functions);
		}

//		get instruction vector for given line(do not expand callInst)
		void getInstsForControlLoc(BasicBlock* controlBB, InstVector& insts)
		{
			// reverse iterator
			for(inst_iterator srcIter = --inst_end(currFunction); srcIter != inst_begin(currFunction); srcIter--)
			{
				Instruction* currInst = &*srcIter;
				if(getDebugLocation(currInst) != controlLoc || currInst->getParent() != controlBB)
				{
					continue;
				}
//				currInst->dump();
				switch(currInst->getOpcode())
				{
				case Instruction::Call:
				{
					insts.push_back(currInst);
					CallInst* callInst = dyn_cast<CallInst>(currInst);
					// ignore subInsts of callInst
					InstVector subInsts;
					unsigned size = getSubInstForCallInst(callInst, subInsts);
					for(unsigned i = 0; i < size; i++)
					{
						srcIter--;
					}
					break;
				}
				default:
				{
					insts.push_back(currInst);
					break;
				}
				}// end of switch
			}
		}// end of getInstsForControlLoc

//		find correlated function info for instructions
		void getFunctionInfoForInsts(InstVector& insts, StringVector& functions)
		{
			for(InstVector::iterator instIter = insts.begin(); instIter != insts.end(); instIter++)
			{
				Instruction* currInst = *instIter;
				switch(currInst->getOpcode())
				{
				case Instruction::Call:
				{
					// record call info
					MySmallString function = getCalledFunctionName(dyn_cast<CallInst>(currInst));
					function += StringRef("_ret");
					if(!function.equals(""))
					{
						functions.push_back(function.str());
					}
					break;
				}
				case Instruction::Load:
				{
					// find function info by data dependence
					MySmallString function = getFunctionInfoForLoadInst(currInst);
					if(!function.equals(""))
					{
						functions.push_back(function);
					}
					break;
				}
				default:
				{
					// do nothing now
					break;
				}
				}// end of switch
			}

		}// end of getFunctionInfoForInsts

//		get function info for one loadInst through data dependence
		MySmallString getFunctionInfoForLoadInst(Instruction* loadInst)
		{
			MySmallString functionRet = StringRef("");
			unsigned minDistanceRet = 0;
			MySmallString functionArg = StringRef("");
			unsigned minDistanceArg = 0;
			MySmallString function;
			unsigned srcLoc = getDebugLocation(loadInst);
			for(inst_iterator dstIter = inst_begin(currFunction); dstIter != inst_end(currFunction); dstIter++)
			{
				Instruction* dstInst = &*dstIter;
				if(dstInst->getOpcode() == Instruction::Call)
				{
					unsigned dstLoc = getDebugLocation(dstInst);
					unsigned distance;
					int temp = srcLoc - dstLoc; //////!!!distance calculation
					if(temp < 0)
						distance = -1 * temp;
					else
						distance = temp;
					CallInst* callInst = dyn_cast<CallInst>(dstInst);
					// skip llvm function...
					function = getCalledFunctionName(callInst);
					if(function.equals(""))
					{
						continue;
					}
					// srcLoc > dstLoc: load after call
					if(temp > 0)
					{
						// call before this load -> return (call -> store -> LOAD)
						Instruction* returnInst = callInst -> getNextNode();
						if(isa<StoreInst>(returnInst))
						{
							std::unique_ptr<Dependence> dependence = dependenceAnalysis->depends(returnInst, loadInst, true);
							// write and read
							if(dependence && dependence->isFlow())
							{
								if(minDistanceRet == 0 || distance < minDistanceRet)
								{
									functionRet = function;
									functionRet += StringRef("_ret");
									minDistanceRet = distance;
									continue;// end this function
								}
							}
						}
					}// end of return

					// call after this control ->  arg(LOAD -> load -> call)
					unsigned numArgs = callInst->getNumArgOperands();
					for(unsigned i = 0; i < numArgs; i++)
					{
						Instruction* argInst = dyn_cast<Instruction>(callInst->getArgOperand(i));
						if(!argInst || argInst->getOpcode() == Instruction::Call)
						{
							continue;
						}
						std::unique_ptr<Dependence> dependence = dependenceAnalysis->depends(argInst, loadInst, true);
						// read and read
						if(dependence && dependence->isInput())
						{
							// reference argument or pointer argument
							if(argInst->getType() && argInst->getType()->isPointerTy())
							{
								if(temp > 0 && (minDistanceRet == 0 || distance < minDistanceRet))
								{
									minDistanceRet = distance;
									functionRet = function;
									functionRet += StringRef("_arg_");
									functionRet += StringRef(std::to_string(i));
									functionRet += StringRef("_ret");
									continue;
								}
							}
							// srcLoc < dstLoc, load after argument
							if(temp < 0 && (minDistanceArg == 0 || distance < minDistanceArg))
							{
								minDistanceArg = distance;
								functionArg = function;
								functionArg += StringRef("_arg_");
								functionArg += StringRef(std::to_string(i));
								continue;// end this function
							}
						}// end of depended arg
					}// end of args
				}// end of call
			}// end of instruction iterator

			//check functionRet and functionArg
			if(functionRet.equals(""))
			{
				return functionArg;
			}
			else
			{
				return functionRet;
			}

		}// end of getFunctionInfoForLoadInst

//		get load instruction for a callInst[no self]
		unsigned getSubInstForCallInst(CallInst* callInst, InstVector& subInsts)
		{
			UserVector users;
			User* user = dyn_cast<User>(callInst);
			if(user)
			{
				users.push_back(user);
			}
			while(users.size() != 0)
			{
				// get front user
				UserVector::iterator beginIter = users.begin();
				User* currUser = *beginIter;
				// remove processes user
				users.erase(beginIter);

				// end flag: different location
				Instruction* tempInst = dyn_cast<Instruction>(currUser);
				if(tempInst && tempInst->getOpcode() == Instruction::Load)
				{
					continue;
				}
				// get value for this user
				for(User::value_op_iterator valueIter = currUser->value_op_begin(); valueIter != currUser->value_op_end(); valueIter++)
				{
					Value* currValue = *valueIter;
					// if value actually user(subUser) -> get value for subUser
					User* tempUser = dyn_cast<User>(currValue);
					if(tempUser)
					{
						users.push_back(tempUser);
					}
					// store loadInst
					tempInst = dyn_cast<Instruction>(currValue);
					if(tempInst)
					{
						subInsts.push_back(tempInst);
					}
				}

			}

			return subInsts.size();
		}

//		get called function name for a call instruction
		MySmallString getCalledFunctionName(CallInst* callInst)
		{
			MySmallString calledFunctionName;
			if(callInst)
			{

				Function* calledFunction = callInst->getCalledFunction();
				if(calledFunction)
				{
					calledFunctionName = calledFunction->getName();
					if(!calledFunctionName.startswith("llvm"))
					{
						return calledFunctionName;
					}
				}
			}

			return StringRef("");
		}

//		get line for a given instruction
		unsigned getDebugLocation(Instruction* inst)
		{
			const DebugLoc& loc = inst->getDebugLoc();
			// start from 1
			if(loc)
			{
				return loc.getLine();
			}
			// no info
			else
			{
				return 0;
			}
		}

//		print or write out function info
		void printFunctionInfo(StringVector& functions)
		{
//			errs() << "functions are: ";
			for(StringVector::iterator functionIter = functions.begin(); functionIter != functions.end(); functionIter++)
			{
				errs() << *functionIter << "\t";
			}
			errs() << "\n";
		}
	};//end of structure

}// end of namespace



char MyTryPass::ID = 0;
static RegisterPass<MyTryPass> X("mypass", "print function and call instruction info");

