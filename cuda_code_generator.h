#pragma once

// Standard C++ includes
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

// GeNN includes
#include "codeGenUtils.h"
#include "codeStream.h"
#include "modelSpec.h"

// NuGeNN includes
#include "code_generator.h"

// CUDACodeGenerator
namespace CUDA
{
class CodeGenerator : public ::CodeGenerator::Base
{
public:
    CodeGenerator(size_t neuronUpdateBlockSize) : m_NeuronUpdateBlockSize(neuronUpdateBlockSize)
    {
    }

    // CodeGenerator::Base virtuals
    virtual void genNeuronUpdateKernel(CodeStream &os, const NNmodel &model,
                                       std::function<void(CodeStream &, const ::CodeGenerator::Base &, const NNmodel&, const NeuronGroup &ng, const std::string &, const std::string &)> handler) const override;

    virtual void genEmitTrueSpike(CodeStream &os, const NNmodel &, const NeuronGroup &, const std::string &neuronID) const override
    {
        genEmitSpike(os, neuronID, "");
    }
    
    virtual void genEmitSpikeLikeEvent(CodeStream &os, const NNmodel &model, const NeuronGroup &ng, const std::string &neuronID) const override
    {
        genEmitSpike(os, neuronID, "Evnt");
    }

    virtual std::string getVarPrefix() const override{ return "dd_"; }

    virtual const std::vector<FunctionTemplate> &getFunctions() const override{ return cudaFunctions; }

private:
    void genParallelNeuronGroup(CodeStream &os, const NNmodel &model,
                                std::function<void(CodeStream &, const ::CodeGenerator::Base &, const NNmodel &, const NeuronGroup&)> handler) const;


    void genEmitSpike(CodeStream &os, const std::string &neuronID, const std::string &suffix) const;

    const size_t m_NeuronUpdateBlockSize;
};
}   // CodeGenerator