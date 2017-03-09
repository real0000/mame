#ifndef _MODEL_H_
#define _MODEL_H_

#include <map>
#include "glm/glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

enum
{
	TEXUSAGE_DIFFUSE = 0,
	TEXUSAGE_NORMAL,
	TEXUSAGE_SPECULAR,
};

class model_file
{
public:
	model_file(std::string a_Filename);
	virtual ~model_file();

	struct model_vertex
	{
		model_vertex()
			: m_Position(0.0f, 0.0f, 0.0f)
			, m_Texcoord(0.0f, 0.0f, 0.0f, 0.0f)
			, m_Normal(0.0f, 0.0f, 0.0f)
			, m_Tangent(0.0f, 0.0f, 0.0f)
			, m_Binormal(0.0f, 0.0f, 0.0f){}

		glm::vec3 m_Position;
		glm::vec4 m_Texcoord;
		glm::vec3 m_Normal;
		glm::vec3 m_Tangent;
		glm::vec3 m_Binormal;
	};
	struct model_meshes
	{
		model_meshes();
		~model_meshes();

		std::string m_Name;
		unsigned int m_Index;
		std::vector<model_vertex> m_Vertex;
		std::vector<unsigned int> m_Indicies;

		std::map<unsigned int, std::string> m_Texures;
		std::vector<unsigned int> m_RefNode;
		glm::vec3 m_BoxSize;
	};
	std::vector<model_meshes *> m_Meshes;

	struct model_node
	{
		model_node() : m_pParent(nullptr), m_NodeName(""), m_Transform(1.0f){}
		~model_node()
		{
			m_Children.clear();
		}

		model_node *m_pParent;
		std::string m_NodeName;
		glm::mat4x4 m_Transform;
		std::vector<unsigned int> m_Children;//index
		std::vector<unsigned int> m_RefMesh;//index
	};
	model_node* find(std::string a_Name) const;

	std::vector<model_node *> m_ModelNodes; // m_pRootNode == m_ModelNodes[0]
	std::map<std::string, int> m_NodeNameMap;
	std::string m_Filename;

private:
	void loadWithFbxSdk(std::string a_Filename);
};

#endif
