/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Implementation of the Diligent::ShaderBase template class

#include <vector>

#include "Atomics.hpp"
#include "ShaderResourceVariable.h"
#include "PipelineState.h"
#include "StringTools.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

template <typename TNameCompare>
SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                    SHADER_RESOURCE_VARIABLE_TYPE     DefaultVariableType,
                                                    const ShaderResourceVariableDesc* Variables,
                                                    Uint32                            NumVars,
                                                    TNameCompare                      NameCompare)
{
    for (Uint32 v = 0; v < NumVars; ++v)
    {
        const auto& CurrVarDesc = Variables[v];
        if (((CurrVarDesc.ShaderStages & ShaderStage) != 0) && NameCompare(CurrVarDesc.Name))
        {
            return CurrVarDesc.Type;
        }
    }
    return DefaultVariableType;
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const Char*                       Name,
                                                           SHADER_RESOURCE_VARIABLE_TYPE     DefaultVariableType,
                                                           const ShaderResourceVariableDesc* Variables,
                                                           Uint32                            NumVars)
{
    return GetShaderVariableType(ShaderStage, DefaultVariableType, Variables, NumVars,
                                 [&](const char* VarName) //
                                 {
                                     return strcmp(VarName, Name) == 0;
                                 });
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const Char*                       Name,
                                                           const PipelineResourceLayoutDesc& LayoutDesc)
{
    return GetShaderVariableType(ShaderStage, Name, LayoutDesc.DefaultVariableType, LayoutDesc.Variables, LayoutDesc.NumVariables);
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const String&                     Name,
                                                           SHADER_RESOURCE_VARIABLE_TYPE     DefaultVariableType,
                                                           const ShaderResourceVariableDesc* Variables,
                                                           Uint32                            NumVars)
{
    return GetShaderVariableType(ShaderStage, DefaultVariableType, Variables, NumVars,
                                 [&](const char* VarName) //
                                 {
                                     return Name.compare(VarName) == 0;
                                 });
}

inline SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType(SHADER_TYPE                       ShaderStage,
                                                           const String&                     Name,
                                                           const PipelineResourceLayoutDesc& LayoutDesc)
{
    return GetShaderVariableType(ShaderStage, Name, LayoutDesc.DefaultVariableType, LayoutDesc.Variables, LayoutDesc.NumVariables);
}

inline bool IsAllowedType(SHADER_RESOURCE_VARIABLE_TYPE VarType, Uint32 AllowedTypeBits) noexcept
{
    return ((1 << VarType) & AllowedTypeBits) != 0;
}

inline Uint32 GetAllowedTypeBit(SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    return 1 << static_cast<Uint32>(VarType);
}

inline Uint32 GetAllowedTypeBits(const SHADER_RESOURCE_VARIABLE_TYPE* AllowedVarTypes, Uint32 NumAllowedTypes) noexcept
{
    if (AllowedVarTypes == nullptr)
        return 0xFFFFFFFF;

    Uint32 AllowedTypeBits = 0;
    for (Uint32 i = 0; i < NumAllowedTypes; ++i)
        AllowedTypeBits |= GetAllowedTypeBit(AllowedVarTypes[i]);
    return AllowedTypeBits;
}

template <typename BufferImplType>
bool VerifyConstantBufferBinding(const PipelineResourceDesc& ResDesc,
                                 Uint32                      ArrayIndex,
                                 const IDeviceObject*        pBuffer,
                                 const BufferImplType*       pBufferImpl,
                                 const IDeviceObject*        pCachedBuffer,
                                 const char*                 ShaderName = nullptr)
{
    if (pBuffer != nullptr && pBufferImpl == nullptr)
    {
        std::stringstream ss;
        ss << "Failed to bind resource '" << pBuffer->GetDesc().Name << "' to variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Invalid resource type: buffer is expected.";
        LOG_ERROR_MESSAGE(ss.str());
        return false;
    }

    bool BindingOK = true;
    if (pBufferImpl != nullptr)
    {
        const auto& BuffDesc = pBufferImpl->GetDesc();
        if ((BuffDesc.BindFlags & BIND_UNIFORM_BUFFER) == 0)
        {
            std::stringstream ss;
            ss << "Error binding buffer '" << BuffDesc.Name << "' to variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
            if (ShaderName != nullptr)
            {
                ss << " in shader '" << ShaderName << '\'';
            }
            ss << ". The buffer was not created with BIND_UNIFORM_BUFFER flag.";
            LOG_ERROR_MESSAGE(ss.str());
            BindingOK = false;
        }

        if (BuffDesc.Usage == USAGE_DYNAMIC && (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS))
        {
            std::stringstream ss;
            ss << "Error binding USAGE_DYNAMIC buffer '" << BuffDesc.Name << "' to variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
            if (ShaderName != nullptr)
            {
                ss << " in shader '" << ShaderName << '\'';
            }
            ss << ". The variable was initialized with PIPELINE_RESOURCE_FLAG_NO_DYNAMIC_BUFFERS flag.";
            LOG_ERROR_MESSAGE(ss.str());
            BindingOK = false;
        }
    }

    if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && pCachedBuffer != nullptr && pCachedBuffer != pBufferImpl)
    {
        auto VarTypeStr = GetShaderVariableTypeLiteralName(ResDesc.VarType);

        std::stringstream ss;
        ss << "Non-null constant (uniform) buffer '" << pCachedBuffer->GetDesc().Name << "' is already bound to " << VarTypeStr
           << " shader variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Attempting to bind ";
        if (pBufferImpl)
        {
            ss << "another resource ('" << pBufferImpl->GetDesc().Name << "')";
        }
        else
        {
            ss << "null";
        }
        ss << " is an error and may cause unpredicted behavior.";

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            ss << " Use another shader resource binding instance or label the variable as dynamic.";

        LOG_ERROR_MESSAGE(ss.str());

        BindingOK = false;
    }

    return BindingOK;
}

template <typename TViewTypeEnum>
const char* GetResourceTypeName();

template <>
inline const char* GetResourceTypeName<TEXTURE_VIEW_TYPE>()
{
    return "texture view";
}

template <>
inline const char* GetResourceTypeName<BUFFER_VIEW_TYPE>()
{
    return "buffer view";
}

inline RESOURCE_DIMENSION GetResourceViewDimension(const ITextureView* pTexView)
{
    VERIFY_EXPR(pTexView != nullptr);
    return pTexView->GetDesc().TextureDim;
}

inline RESOURCE_DIMENSION GetResourceViewDimension(const IBufferView* /*pBuffView*/)
{
    return RESOURCE_DIM_BUFFER;
}

inline Uint32 GetResourceSampleCount(const ITextureView* pTexView)
{
    VERIFY_EXPR(pTexView != nullptr);
    return const_cast<ITextureView*>(pTexView)->GetTexture()->GetDesc().SampleCount;
}

inline Uint32 GetResourceSampleCount(const IBufferView* /*pBuffView*/)
{
    return 0;
}



template <typename TextureViewImplType>
bool ValidateResourceViewDimension(const char*                ResName,
                                   Uint32                     ArraySize,
                                   Uint32                     ArrayInd,
                                   const TextureViewImplType* pViewImpl,
                                   RESOURCE_DIMENSION         ExpectedResourceDim,
                                   bool                       IsMultisample)
{
    bool BindingsOK = true;

    if (ExpectedResourceDim != RESOURCE_DIM_UNDEFINED)
    {
        const auto ResourceDim = GetResourceViewDimension(pViewImpl);
        if (ResourceDim != ExpectedResourceDim)
        {
            LOG_ERROR_MESSAGE("The dimension of resource view '", pViewImpl->GetDesc().Name,
                              "' bound to variable '", GetShaderResourcePrintName(ResName, ArraySize, ArrayInd), "' is ", GetResourceDimString(ResourceDim),
                              ", but resource dimension expected by the shader is ", GetResourceDimString(ExpectedResourceDim), ".");
        }

        if (ResourceDim == RESOURCE_DIM_TEX_2D || ResourceDim == RESOURCE_DIM_TEX_2D_ARRAY)
        {
            auto SampleCount = GetResourceSampleCount(pViewImpl);
            if (IsMultisample && SampleCount == 1)
            {
                LOG_ERROR_MESSAGE("Texture view '", pViewImpl->GetDesc().Name, "' bound to variable '",
                                  GetShaderResourcePrintName(ResName, ArraySize, ArrayInd), "' is invalid: multisample texture is expected.");
                BindingsOK = false;
            }
            else if (!IsMultisample && SampleCount > 1)
            {
                LOG_ERROR_MESSAGE("Texture view '", pViewImpl->GetDesc().Name, "' bound to variable '",
                                  GetShaderResourcePrintName(ResName, ArraySize, ArrayInd), "' is invalid: single-sample texture is expected.");
                BindingsOK = false;
            }
        }
    }

    return BindingsOK;
}


template <typename ResourceViewImplType,
          typename ViewTypeEnumType>
bool VerifyResourceViewBinding(const PipelineResourceDesc&             ResDesc,
                               Uint32                                  ArrayIndex,
                               const IDeviceObject*                    pView,
                               const ResourceViewImplType*             pViewImpl,
                               std::initializer_list<ViewTypeEnumType> ExpectedViewTypes,
                               RESOURCE_DIMENSION                      ExpectedResourceDimension,
                               bool                                    IsMultisample,
                               const IDeviceObject*                    pCachedView,
                               const char*                             ShaderName = nullptr)
{
    const char* ExpectedResourceType = GetResourceTypeName<ViewTypeEnumType>();

    if (pView != nullptr && pViewImpl == nullptr)
    {
        std::stringstream ss;
        ss << "Failed to bind resource '" << pView->GetDesc().Name << "' to variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Invalid resource type: " << ExpectedResourceType << " is expected.";
        LOG_ERROR_MESSAGE(ss.str());
        return false;
    }

    bool BindingOK = true;
    if (pViewImpl)
    {
        auto ViewType           = pViewImpl->GetDesc().ViewType;
        bool IsExpectedViewType = false;
        for (auto ExpectedViewType : ExpectedViewTypes)
        {
            if (ExpectedViewType == ViewType)
                IsExpectedViewType = true;
        }

        if (!IsExpectedViewType)
        {
            std::stringstream ss;
            ss << "Error binding " << ExpectedResourceType << " '" << pViewImpl->GetDesc().Name << "' to variable '"
               << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
            if (ShaderName != nullptr)
            {
                ss << " in shader '" << ShaderName << '\'';
            }
            ss << ". Incorrect view type: ";
            bool IsFirstViewType = true;
            for (auto ExpectedViewType : ExpectedViewTypes)
            {
                if (!IsFirstViewType)
                {
                    ss << " or ";
                }
                ss << GetViewTypeLiteralName(ExpectedViewType);
                IsFirstViewType = false;
            }
            ss << " is expected, " << GetViewTypeLiteralName(ViewType) << " is provided.";
            LOG_ERROR_MESSAGE(ss.str());

            BindingOK = false;
        }

        if (!ValidateResourceViewDimension(ResDesc.Name, ResDesc.ArraySize, ArrayIndex, pViewImpl, ExpectedResourceDimension, IsMultisample))
        {
            BindingOK = false;
        }
    }

    if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && pCachedView != nullptr && pCachedView != pViewImpl)
    {
        const auto* VarTypeStr = GetShaderVariableTypeLiteralName(ResDesc.VarType);

        std::stringstream ss;
        ss << "Non-null resource '" << pCachedView->GetDesc().Name << "' is already bound to " << VarTypeStr
           << " shader variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Attempting to bind ";
        if (pViewImpl)
        {
            ss << "another resource ('" << pViewImpl->GetDesc().Name << "')";
        }
        else
        {
            ss << "null";
        }
        ss << " is an error and may cause unpredicted behavior.";

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            ss << " Use another shader resource binding instance or label the variable as dynamic.";

        LOG_ERROR_MESSAGE(ss.str());

        BindingOK = false;
    }

    return BindingOK;
}

template <typename BufferViewImplType>
bool ValidateBufferMode(const PipelineResourceDesc& ResDesc,
                        Uint32                      ArrayIndex,
                        const BufferViewImplType*   pBufferView)
{
    bool BindingOK = true;
    if (pBufferView != nullptr)
    {
        const auto& BuffDesc = pBufferView->GetBuffer()->GetDesc();
        if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER)
        {
            if (BuffDesc.Mode != BUFFER_MODE_FORMATTED)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", pBufferView->GetDesc().Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  GetShaderResourcePrintName(ResDesc, ArrayIndex), ": formatted buffer view is expected.");
                BindingOK = false;
            }
        }
        else
        {
            if (BuffDesc.Mode != BUFFER_MODE_STRUCTURED && BuffDesc.Mode != BUFFER_MODE_RAW)
            {
                LOG_ERROR_MESSAGE("Error binding buffer view '", pBufferView->GetDesc().Name, "' of buffer '", BuffDesc.Name, "' to shader variable '",
                                  GetShaderResourcePrintName(ResDesc, ArrayIndex), ": structured or raw buffer view is expected.");
                BindingOK = false;
            }
        }
    }

    return BindingOK;
}


template <typename TLASImplType>
bool VerifyTLASResourceBinding(const PipelineResourceDesc& ResDesc,
                               Uint32                      ArrayIndex,
                               const IDeviceObject*        pTLAS,
                               const TLASImplType*         pTLASImpl,
                               const IDeviceObject*        pCachedAS,
                               const char*                 ShaderName = nullptr)
{
    if (pTLAS != nullptr && pTLASImpl == nullptr)
    {
        std::stringstream ss;
        ss << "Failed to bind resource '" << pCachedAS->GetDesc().Name << "' to variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Invalid resource type: TLAS is expected.";
        LOG_ERROR_MESSAGE(ss.str());
        return false;
    }

    bool BindingOK = true;

    if (ResDesc.VarType != SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC && pCachedAS != nullptr && pCachedAS != pTLAS)
    {
        const auto* VarTypeStr = GetShaderVariableTypeLiteralName(ResDesc.VarType);

        std::stringstream ss;
        ss << "Non-null resource '" << pCachedAS->GetDesc().Name << "' is already bound to " << VarTypeStr
           << " shader variable '" << GetShaderResourcePrintName(ResDesc, ArrayIndex) << '\'';
        if (ShaderName != nullptr)
        {
            ss << " in shader '" << ShaderName << '\'';
        }
        ss << ". Attempting to bind ";
        if (pTLAS)
        {
            ss << "another resource ('" << pTLAS->GetDesc().Name << "')";
        }
        else
        {
            ss << "null";
        }
        ss << " is an error and may cause unpredicted behavior.";

        if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            ss << " Use another shader resource binding instance or label the variable as dynamic.";

        LOG_ERROR_MESSAGE(ss.str());

        BindingOK = false;
    }

    return BindingOK;
}

inline void VerifyAndCorrectSetArrayArguments(const char* Name, Uint32 ArraySize, Uint32& FirstElement, Uint32& NumElements)
{
    if (FirstElement >= ArraySize)
    {
        LOG_ERROR_MESSAGE("SetArray arguments are invalid for '", Name, "' variable: FirstElement (", FirstElement, ") is out of allowed range 0 .. ", ArraySize - 1);
        FirstElement = ArraySize - 1;
        NumElements  = 0;
    }

    if (FirstElement + NumElements > ArraySize)
    {
        LOG_ERROR_MESSAGE("SetArray arguments are invalid for '", Name, "' variable: specified element range (", FirstElement, " .. ",
                          FirstElement + NumElements - 1, ") is out of array bounds 0 .. ", ArraySize - 1);
        NumElements = ArraySize - FirstElement;
    }
}

template <typename ShaderVectorType>
std::string GetShaderGroupName(const ShaderVectorType& Shaders)
{
    std::string Name;
    if (Shaders.size() == 1)
    {
        Name = Shaders[0]->GetDesc().Name;
    }
    else
    {
        Name = "{";
        for (size_t s = 0; s < Shaders.size(); ++s)
        {
            if (s > 0)
                Name += ", ";
            Name += Shaders[s]->GetDesc().Name;
        }
        Name += "}";
    }
    return Name;
}

/// Base implementation of a shader variable
template <typename ThisImplType,
          typename VarManagerType,
          typename ResourceVariableBaseInterface = IShaderResourceVariable>
struct ShaderVariableBase : public ResourceVariableBaseInterface
{
    ShaderVariableBase(VarManagerType& ParentManager, Uint32 ResIndex) :
        m_ParentManager{ParentManager},
        m_ResIndex{ResIndex}
    {
    }

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override
    {
        if (ppInterface == nullptr)
            return;

        *ppInterface = nullptr;
        if (IID == IID_ShaderResourceVariable || IID == IID_Unknown)
        {
            *ppInterface = this;
            (*ppInterface)->AddRef();
        }
    }

    virtual Atomics::Long DILIGENT_CALL_TYPE AddRef() override final
    {
        return m_ParentManager.GetOwner().AddRef();
    }

    virtual Atomics::Long DILIGENT_CALL_TYPE Release() override final
    {
        return m_ParentManager.GetOwner().Release();
    }

    virtual IReferenceCounters* DILIGENT_CALL_TYPE GetReferenceCounters() const override final
    {
        return m_ParentManager.GetOwner().GetReferenceCounters();
    }

    virtual void DILIGENT_CALL_TYPE Set(IDeviceObject* pObject) override final
    {
        static_cast<ThisImplType*>(this)->BindResource(pObject, 0);
    }

    virtual void DILIGENT_CALL_TYPE SetArray(IDeviceObject* const* ppObjects,
                                             Uint32                FirstElement,
                                             Uint32                NumElements) override final
    {
        const auto& Desc = GetDesc();
        VerifyAndCorrectSetArrayArguments(Desc.Name, Desc.ArraySize, FirstElement, NumElements);
        for (Uint32 elem = 0; elem < NumElements; ++elem)
            static_cast<ThisImplType*>(this)->BindResource(ppObjects[elem], FirstElement + elem);
    }

    virtual SHADER_RESOURCE_VARIABLE_TYPE DILIGENT_CALL_TYPE GetType() const override final
    {
        return GetDesc().VarType;
    }

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(ShaderResourceDesc& ResourceDesc) const override final
    {
        const auto& Desc       = GetDesc();
        ResourceDesc.Name      = Desc.Name;
        ResourceDesc.Type      = Desc.ResourceType;
        ResourceDesc.ArraySize = Desc.ArraySize;
    }

    virtual Uint32 DILIGENT_CALL_TYPE GetIndex() const override final
    {
        return m_ParentManager.GetVariableIndex(*static_cast<const ThisImplType*>(this));
    }

    void BindResources(IResourceMapping* pResourceMapping, Uint32 Flags)
    {
        auto* const pThis = static_cast<ThisImplType*>(this);

        const auto& ResDesc = pThis->GetDesc();

        if ((Flags & (1u << ResDesc.VarType)) == 0)
            return;

        for (Uint32 ArrInd = 0; ArrInd < ResDesc.ArraySize; ++ArrInd)
        {
            if ((Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) != 0 && pThis->IsBound(ArrInd))
                continue;

            RefCntAutoPtr<IDeviceObject> pObj;
            pResourceMapping->GetResource(ResDesc.Name, &pObj, ArrInd);
            if (pObj)
            {
                pThis->BindResource(pObj, ArrInd);
            }
            else
            {
                if ((Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !pThis->IsBound(ArrInd))
                {
                    LOG_ERROR_MESSAGE("Unable to bind resource to shader variable '",
                                      GetShaderResourcePrintName(ResDesc, ArrInd),
                                      "': resource is not found in the resource mapping. "
                                      "Do not use BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED flag to suppress the message if this is not an issue.");
                }
            }
        }
    }

    const PipelineResourceDesc& GetDesc() const { return m_ParentManager.GetResourceDesc(m_ResIndex); }

protected:
    // Variable manager that owns this variable
    VarManagerType& m_ParentManager;

    // Resource index in pipeline resource signature m_Desc.Resources[]
    const Uint32 m_ResIndex;
};

} // namespace Diligent
