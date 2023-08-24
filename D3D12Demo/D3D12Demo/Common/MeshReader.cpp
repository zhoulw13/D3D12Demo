#include "MeshReader.h"


enum READ_MODE
{
	VERTEX,
	INDICES,
	IDLE
};

void MeshReader::LoadFromTxt(const std::string filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{
	std::string line;
	std::ifstream file(filename);

	READ_MODE state = READ_MODE::IDLE;
	if (file.is_open())
	{
		while (getline(file, line))
		{
			if (state == IDLE)
			{
				size_t pos = line.find_first_of(":");
				if (pos == std::string::npos)
				{
					pos = line.find_first_of(" ");
				}
				std::string keyword = line.substr(0, pos);
				std::cout << keyword << std::endl;
				if (keyword.compare("VertexCount") == 0)
				{
					std::stringstream ss(line.substr(pos + 1));
					int verticeCount;
					ss >> verticeCount;
					std::cout << verticeCount << std::endl;
				}
				else if (keyword.compare("VertexList") == 0)
				{
					state = VERTEX;
					getline(file, line);
				}
				else if (keyword.compare("TriangleList") == 0)
				{
					state = INDICES;
					getline(file, line);
				}
			}
			else if (state == VERTEX)
			{
				if (line[0] == '}')
				{
					state = IDLE;
					continue;
				}

				std::stringstream ss(line);
				Vertex v;
				ss >> v.Pos.x >> v.Pos.y >> v.Pos.z >> v.Normal.x >> v.Normal.y >> v.Normal.z;
				vertices.push_back(v);
			}
			else if (state == INDICES)
			{
				if (line[0] == '}')
				{
					state = IDLE;
					continue;
				}

				std::stringstream ss(line);
				uint32_t a, b, c;
				ss >> a >> b >> c;
				indices.push_back(a);
				indices.push_back(b);
				indices.push_back(c);
			}

		}
		file.close();
	}
}