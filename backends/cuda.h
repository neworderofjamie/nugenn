#pragma once

// Standard C++ includes
#include <functional>
#include <map>
#include <string>

// CUDA includes
#include <cuda.h>
#include <cuda_runtime.h>

// GeNN includes
#include "codeStream.h"

// NuGeNN includes
#include "base.h"
#include "../substitution_stack.h"

//--------------------------------------------------------------------------
// CodeGenerator::Backends::CUDA
//--------------------------------------------------------------------------
namespace CodeGenerator
{
namespace Backends
{
class CUDA : public Base
{
public:
    CUDA(size_t neuronUpdateBlockSize, size_t presynapticUpdateBlockSize, size_t initBlockSize, size_t initSparseBlockSize,
         int localHostID, const Base &hostBackend);

    //--------------------------------------------------------------------------
    // CodeGenerator::Backends:: virtuals
    //--------------------------------------------------------------------------
    virtual void genNeuronUpdate(CodeStream &os, const NNmodel &model, NeuronGroupHandler handler) const override;

    virtual void genPresynapticUpdate(CodeStream &os, const NNmodel &model,
                                      SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler) const override;

    virtual void genInit(CodeStream &os, const NNmodel &model,
                         NeuronGroupHandler localNGHandler, NeuronGroupHandler remoteNGHandler,
                         SynapseGroupHandler sgDenseInitHandler, SynapseGroupHandler sgSparseConnectHandler, 
                         SynapseGroupHandler sgSparseInitHandler) const override;

    virtual void genRunnerPreamble(CodeStream &os) const override;

    virtual void genVariableDefinition(CodeStream &os, const std::string &type, const std::string &name, VarMode mode) const override;
    virtual void genVariableImplementation(CodeStream &os, const std::string &type, const std::string &name, VarMode mode) const override;
    virtual void genVariableAllocation(CodeStream &os, const std::string &type, const std::string &name, VarMode mode, size_t count) const override;
    virtual void genVariableFree(CodeStream &os, const std::string &name, VarMode mode) const override;

    virtual void genPopVariableInit(CodeStream &os, VarMode mode, const Substitutions &kernelSubs, Handler handler) const override;
    virtual void genVariableInit(CodeStream &os, VarMode mode, size_t count, const std::string &countVarName,
                                 const Substitutions &kernelSubs, Handler handler) const override;

    virtual void genEmitTrueSpike(CodeStream &os, const NNmodel&, const NeuronGroup&, const Substitutions &subs) const override
    {
        genEmitSpike(os, subs, "");
    }
    
    virtual void genEmitSpikeLikeEvent(CodeStream &os, const NNmodel &, const NeuronGroup &, const Substitutions &subs) const override
    {
        genEmitSpike(os, subs, "Evnt");
    }

    virtual std::string getVarPrefix() const override{ return "dd_"; }

private:
    template<typename T>
    using GetPaddedGroupSizeFunc = std::function<size_t(const T&)>;

    template<typename T>
    using FilterGroupFunc = std::function<bool(const T&)>;

    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------
    template<typename T>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::map<std::string, T> &groups, size_t &idStart,
                          GetPaddedGroupSizeFunc<T> getPaddedSizeFunc,
                          FilterGroupFunc<T> filter, 
                          GroupHandler<T> handler) const
    {
        // Populate neuron update groups
        Substitutions popSubs(&kernelSubs);
        for (const auto &g : groups) {
            // If this synapse group should be processed
            if(filter(g.second)) {
                const size_t paddedSize = getPaddedSizeFunc(g.second);

                os << "// " << g.first << std::endl;

                // If this is the first  group
                if (idStart == 0) {
                    os << "if(id < " << paddedSize << ")" << CodeStream::OB(1);
                    popSubs.addVarSubstitution("id", "id");
                }
                else {
                    os << "if(id >= " << idStart << " && id < " << idStart + paddedSize << ")" << CodeStream::OB(1);
                    os << "const unsigned int lid = id - " << idStart << ";" << std::endl;
                    popSubs.addVarSubstitution("id", "lid");
                }

                handler(os, g.second, popSubs);

                idStart += paddedSize;
                os << CodeStream::CB(1) << std::endl;
            }
        }
    }

    template<typename T>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::map<std::string, T> &groups, size_t &idStart,
                          GetPaddedGroupSizeFunc<T> getPaddedSizeFunc,
                          GroupHandler<T> handler) const
    {
        genParallelGroup<T>(os, kernelSubs, groups, idStart, getPaddedSizeFunc,
                            [](const T&){ return true; }, handler);
    }
                                 
    void genEmitSpike(CodeStream &os, const Substitutions &subs, const std::string &suffix) const;

    void genPresynapticUpdatePreSpan(CodeStream &os, const NNmodel &model, const SynapseGroup &sg, const Substitutions &popSubs, bool trueSpike,
                                     SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler) const;
    void genPresynapticUpdatePostSpan(CodeStream &os, const NNmodel &model, const SynapseGroup &sg, const Substitutions &popSubs, bool trueSpike,
                                      SynapseGroupHandler wumThreshHandler, SynapseGroupHandler wumSimHandler) const;

    size_t getPresynapticUpdateKernelSize(const SynapseGroup &sg) const;
    
    bool shouldAccumulateInLinSyn(const SynapseGroup &sg) const;
    
    bool shouldAccumulateInSharedMemory(const SynapseGroup &sg) const;
    
    std::string getFloatAtomicAdd(const std::string &ftype) const;
    
    const cudaDeviceProp &getChosenCUDADevice() const{ return m_Devices[m_ChosenDevice]; }
    
    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const Base &m_HostBackend;
    
    const size_t m_NeuronUpdateBlockSize;
    const size_t m_PresynapticUpdateBlockSize;
    const size_t m_InitBlockSize;
    const size_t m_InitSparseBlockSize;
    const int m_LocalHostID;
    
    std::vector<cudaDeviceProp> m_Devices;
    int m_ChosenDevice;
};
}   // Backends
}   // CodeGenerator
