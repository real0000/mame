#include "emu.h"
#include "fbxsdk.h"
#include "modelfile.h"
 
#pragma region model_file

//
// fbx sdk help function
//
template<typename SrcType, typename TVec>
static void setupVertexData(FbxMesh *a_pSrcMesh, SrcType *a_pSrcData, model_file::model_meshes *a_pTargetMesh, std::function<void(model_file::model_vertex&, TVec)> a_Lambda)
{
    if( nullptr == a_pSrcData ) return;

    switch( a_pSrcData->GetMappingMode() )
    {
        case FbxLayerElement::eByControlPoint:{
            auto &l_DirectArray = a_pSrcData->GetDirectArray();
            switch( a_pSrcData->GetReferenceMode() )
            {
                case FbxLayerElement::eDirect:{
                    for( unsigned int i=0 ; i<a_pSrcMesh->GetControlPointsCount() ; ++i ) a_Lambda(a_pTargetMesh->m_Vertex[i], l_DirectArray[i]);
                    }break;

                case FbxLayerElement::eIndexToDirect:{
                    auto &l_IndexArray = a_pSrcData->GetIndexArray();
                    for( unsigned int i=0 ; i<l_IndexArray.GetCount() ; ++i ) a_Lambda(a_pTargetMesh->m_Vertex[i], l_DirectArray[l_IndexArray[i]]);
                    }break;

                default:break;
            }
            }break;

        case FbxLayerElement::eByPolygonVertex:{
            auto &l_DirectArray = a_pSrcData->GetDirectArray();
            switch( a_pSrcData->GetReferenceMode() )
            {
                case FbxLayerElement::eDirect:{
                    for( unsigned int i=0 ; i<a_pSrcMesh->GetPolygonCount() ; ++i )
                    {
                        for( unsigned int j=0 ; j<3 ; ++j )
                        {
                            unsigned int l_PtIdx = a_pSrcMesh->GetPolygonVertex(i, j);
                            a_Lambda(a_pTargetMesh->m_Vertex[l_PtIdx], l_DirectArray[i*3 + j]);
                        }
                    }
                    }break;

                case FbxLayerElement::eIndexToDirect:{
                    auto &l_IndexArray = a_pSrcData->GetIndexArray();
                    for( unsigned int i=0 ; i<a_pSrcMesh->GetPolygonCount() ; ++i )
                    {
                        for( unsigned int j=0 ; j<3 ; ++j )
                        {
                            unsigned int l_PtIdx = a_pSrcMesh->GetPolygonVertex(i, j);
                            a_Lambda(a_pTargetMesh->m_Vertex[l_PtIdx], l_DirectArray[l_IndexArray[i*3 + j]]);
                        }
                    }
                    }break;
            }
            }break;

        default:break;
    }
}

static std::string getFbxMaterialTexture(FbxSurfaceMaterial *a_pSrcMaterial, const char *a_pTypeName)
{
    FbxProperty l_DiffTexture = a_pSrcMaterial->FindProperty(a_pTypeName);
    if( l_DiffTexture.IsValid() )
    {
        FbxLayeredTexture *l_pLayeredTex = 0 != l_DiffTexture.GetSrcObjectCount<FbxLayeredTexture>() ? l_DiffTexture.GetSrcObject<FbxLayeredTexture>(0) : nullptr;
        if( nullptr != l_pLayeredTex )
        {
            FbxFileTexture *l_pTexFile = 0 != l_pLayeredTex->GetSrcObjectCount<FbxFileTexture>() ? l_pLayeredTex->GetSrcObject<FbxFileTexture>(0) : nullptr;
            return nullptr == l_pTexFile ? "" : l_pTexFile->GetRelativeFileName();
        }
    }
    
    FbxFileTexture *l_pTexFile = 0 != l_DiffTexture.GetSrcObjectCount<FbxFileTexture>() ? l_DiffTexture.GetSrcObject<FbxFileTexture>(0) : nullptr;
    return nullptr == l_pTexFile ? "" : l_pTexFile->GetRelativeFileName();
}

//
// model_file::model_meshes
//
model_file::model_meshes::model_meshes()
    : m_Index(0)
    , m_BoxSize(0.0f, 0.0f, 0.0f)
{
}

model_file::model_meshes::~model_meshes()
{
}
//
// model_file
//
model_file::model_file(std::string a_File)
    : m_Filename(a_File)
{
    loadWithFbxSdk(a_File);
}

model_file::~model_file()
{
    for( unsigned int i=0 ; i<m_Meshes.size() ; i++ ) delete m_Meshes[i];
    m_Meshes.clear();

    for( unsigned int i=0 ; i<m_ModelNodes.size() ; i++ ) delete m_ModelNodes[i];
    m_ModelNodes.clear();
}

model_file::model_node* model_file::find(std::string a_Name) const
{
    auto it = m_NodeNameMap.find(a_Name);
    if( m_NodeNameMap.end() == it ) return nullptr;
    return m_ModelNodes[it->second];
}

void model_file::loadWithFbxSdk(std::string a_Filename)
{
    FbxManager *l_pSdkManager = FbxManager::Create();
    FbxIOSettings *l_pIOCfg = FbxIOSettings::Create(l_pSdkManager, IOSROOT);
    l_pIOCfg->SetBoolProp(IMP_FBX_MATERIAL, true);
    l_pIOCfg->SetBoolProp(IMP_FBX_TEXTURE, true);
    l_pIOCfg->SetBoolProp(IMP_FBX_LINK, false);
    l_pIOCfg->SetBoolProp(IMP_FBX_SHAPE, false);
    l_pIOCfg->SetBoolProp(IMP_FBX_GOBO, false);
    l_pIOCfg->SetBoolProp(IMP_FBX_ANIMATION, false);
    l_pIOCfg->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
    l_pSdkManager->SetIOSettings(l_pIOCfg);

    FbxImporter* l_pImporter = FbxImporter::Create(l_pSdkManager, "");
    bool l_bSuccess = l_pImporter->Initialize(a_Filename.c_str(), -1, l_pIOCfg);
    assert(l_bSuccess);

    FbxScene* l_pScene = FbxScene::Create(l_pSdkManager, "SourceScene");
    l_pImporter->Import(l_pScene);
    l_pImporter->Destroy();

    std::map<FbxNode *, unsigned int> l_NodeIdxMap;
    std::map<FbxMesh *, unsigned int> l_MeshMap;
    {
        m_ModelNodes.resize(l_pScene->GetNodeCount(), nullptr);
        for( unsigned int i=0 ; i<m_ModelNodes.size() ; ++i )
        {
            m_ModelNodes[i] = new model_node();
            FbxNode *l_pCurrNode = l_pScene->GetNode(i);
            l_NodeIdxMap[l_pCurrNode] = i;

            FbxAMatrix l_FbxMat = l_pCurrNode->EvaluateLocalTransform();

            m_ModelNodes[i]->m_NodeName = l_pCurrNode->GetName();
            m_NodeNameMap[m_ModelNodes[i]->m_NodeName] = i;
            for( unsigned int j=0 ; j<4 ; ++j )
            {
                FbxVector4 l_Data = l_FbxMat.GetColumn(j);
                m_ModelNodes[i]->m_Transform[j] = glm::vec4(l_Data[0], l_Data[1], l_Data[2], l_Data[3]);
            }
            //m_ModelNodes[i]->m_Transform = glm::transpose(m_ModelNodes[i]->m_Transform);

            FbxNodeAttribute *l_pNodeAttr = l_pCurrNode->GetNodeAttribute();
            if( nullptr != l_pNodeAttr )
            {
                switch( l_pNodeAttr->GetAttributeType() )
                {
                    case FbxNodeAttribute::eMesh:{
                        FbxMesh *l_pMesh = (FbxMesh *)l_pNodeAttr;

                        unsigned int l_MeshIdx = 0;
                        auto l_MeshIt = l_MeshMap.find(l_pMesh);
                        if( l_MeshMap.find(l_pMesh) == l_MeshMap.end() )
                        {
                            l_MeshIdx = l_MeshMap.size();
                            l_MeshMap.insert(std::make_pair(l_pMesh, l_MeshIdx));
                        }
                        else l_MeshIdx = l_MeshIt->second;
                        m_ModelNodes[i]->m_RefMesh.push_back(l_MeshIdx);
                        }break;

                    default:break;
                }
            }
        }

        for( unsigned int i=0 ; i<m_ModelNodes.size() ; ++i )
        {
            FbxNode *l_pCurrNode = l_pScene->GetNode(i);
            if( nullptr != l_pCurrNode->GetParent() ) m_ModelNodes[i]->m_pParent = m_ModelNodes[l_NodeIdxMap[l_pCurrNode->GetParent()]];
            for( unsigned int j=0 ; j<l_pCurrNode->GetChildCount() ; ++j )
            {
                unsigned int l_ChildIdx = l_NodeIdxMap[l_pCurrNode->GetChild(j)];
                m_ModelNodes[i]->m_Children.push_back(l_ChildIdx);
            }
        }
    }

    m_Meshes.resize(l_MeshMap.size(), nullptr);
    for( auto it = l_MeshMap.begin() ; it != l_MeshMap.end() ; ++it )
    {
        FbxMesh *l_pSrcMesh = it->first;
        unsigned int l_Idx = it->second;
        m_Meshes[l_Idx] = new model_meshes();
        m_Meshes[l_Idx]->m_Name = it->first->GetName();
        m_Meshes[l_Idx]->m_Index = l_Idx;

        FbxNode *l_pRefNode = it->first->GetNode();
        m_Meshes[l_Idx]->m_RefNode.push_back(l_NodeIdxMap[l_pRefNode]);

        FbxVector4 l_FbxBoxMax, l_FbxBoxMin, l_FbxBoxCenter;
        l_pRefNode->EvaluateGlobalBoundingBoxMinMaxCenter(l_FbxBoxMin, l_FbxBoxMax, l_FbxBoxCenter);
        l_FbxBoxMax -= l_FbxBoxMin;
        m_Meshes[l_Idx]->m_BoxSize = glm::vec3(l_FbxBoxMax[0], l_FbxBoxMax[1], l_FbxBoxMax[2]) * 0.5f;

        unsigned int l_NumVtx = l_pSrcMesh->GetControlPointsCount();
        m_Meshes[l_Idx]->m_Vertex.resize(l_NumVtx);
        FbxVector4 l_Trans = l_pRefNode->GetGeometricTranslation(FbxNode::eSourcePivot);
        FbxVector4 l_Scale = l_pRefNode->GetGeometricScaling(FbxNode::eSourcePivot);
        FbxVector4 l_Rot = l_pRefNode->GetGeometricRotation(FbxNode::eSourcePivot);
        FbxAMatrix l_VtxTrans;
        l_VtxTrans.SetTRS(l_Trans, l_Rot, l_Scale);
        glm::mat4x4 l_MultiMat;
        for( unsigned int i=0 ; i<4 ; ++i )
        {
            FbxVector4 l_SrcVec = l_VtxTrans.GetColumn(i);
            l_MultiMat[i] = glm::vec4(l_SrcVec[0], l_SrcVec[1], l_SrcVec[2], l_SrcVec[3]);
        }
        for( unsigned int i=0 ; i<l_NumVtx ; ++i )
        {
            FbxVector4 l_SrcVtx = l_pSrcMesh->GetControlPoints()[i];
            glm::vec4 l_Temp = glm::vec4(l_SrcVtx[0], l_SrcVtx[1], l_SrcVtx[2], 1.0f) * l_MultiMat;
            model_vertex &l_TargetVtx = m_Meshes[l_Idx]->m_Vertex[i];
            l_TargetVtx.m_Position = glm::vec3(l_Temp[0], l_Temp[1], l_Temp[2]);
        }
        
        std::function<void(model_vertex&, FbxVector4)> l_SetNormalFunc = [](model_vertex &a_Vtx, FbxVector4 a_Src){ a_Vtx.m_Normal = glm::vec3(a_Src[0], a_Src[1], a_Src[2]); };
        setupVertexData(l_pSrcMesh, l_pSrcMesh->GetLayer(0)->GetNormals(), m_Meshes[l_Idx], l_SetNormalFunc);

        std::function<void(model_vertex&, FbxVector4)> l_SetTangentFunc = [](model_vertex &a_Vtx, FbxVector4 a_Src){ a_Vtx.m_Tangent = glm::vec3(a_Src[0], a_Src[1], a_Src[2]); };
        setupVertexData(l_pSrcMesh, l_pSrcMesh->GetLayer(0)->GetTangents(), m_Meshes[l_Idx], l_SetTangentFunc);

        std::function<void(model_vertex&, FbxVector4)> l_SetBinormalFunc = [](model_vertex &a_Vtx, FbxVector4 a_Src){ a_Vtx.m_Binormal = glm::vec3(a_Src[0], a_Src[1], a_Src[2]); };
        setupVertexData(l_pSrcMesh, l_pSrcMesh->GetLayer(0)->GetBinormals(), m_Meshes[l_Idx], l_SetBinormalFunc);

        std::function<void(model_vertex&, FbxVector2)> l_SetUVFunc = [](model_vertex &a_Vtx, FbxVector2 a_Src){ a_Vtx.m_Texcoord.x = a_Src[0]; a_Vtx.m_Texcoord.y = a_Src[1]; };
        setupVertexData(l_pSrcMesh, l_pSrcMesh->GetLayer(0)->GetUVs(), m_Meshes[l_Idx], l_SetUVFunc);

        if( l_pSrcMesh->GetLayerCount() > 1 )
        {
            std::function<void(model_vertex&, FbxVector2)> l_SetUV2Func = [](model_vertex &a_Vtx, FbxVector2 a_Src){ a_Vtx.m_Texcoord.z = a_Src[0]; a_Vtx.m_Texcoord.w = a_Src[1]; };
            setupVertexData(l_pSrcMesh, l_pSrcMesh->GetLayer(1)->GetUVs(), m_Meshes[l_Idx], l_SetUV2Func);
        }

        for( unsigned int i=0 ; i<l_pSrcMesh->GetPolygonCount() ; ++i )
        {
            for( unsigned int j=0 ; j<3 ; ++j )
            {
                unsigned int l_PtIdx = l_pSrcMesh->GetPolygonVertex(i, j);
                m_Meshes[l_Idx]->m_Indicies.push_back(l_PtIdx);        
            }
        }

        if( 0 != l_pRefNode->GetMaterialCount() )
        {
            FbxSurfaceMaterial *l_pSrcMaterial = l_pRefNode->GetMaterial(0);
            m_Meshes[l_Idx]->m_Texures[TEXUSAGE_DIFFUSE] = getFbxMaterialTexture(l_pSrcMaterial, FbxSurfaceMaterial::sDiffuse);
            
            std::string l_TexName(getFbxMaterialTexture(l_pSrcMaterial, FbxSurfaceMaterial::sNormalMap));
            if( 0 != l_TexName.length() ) m_Meshes[l_Idx]->m_Texures[TEXUSAGE_NORMAL] = l_TexName;

            l_TexName = getFbxMaterialTexture(l_pSrcMaterial, FbxSurfaceMaterial::sSpecular);
            if( 0 != l_TexName.length() ) m_Meshes[l_Idx]->m_Texures[TEXUSAGE_SPECULAR] = l_TexName;
        }
    }

    l_pScene->Clear();
}
#pragma endregion
