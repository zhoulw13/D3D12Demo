#pragma once

#include "Util.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

class MeshReader
{
public:
	static void LoadFromTxt(const std::string filename, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);
};