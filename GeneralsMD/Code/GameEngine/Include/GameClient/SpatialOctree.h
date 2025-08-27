/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// Forward declarations
class Drawable;
struct Region3D;
struct Coord3D;

// Minimal spatial octree for efficient drawable queries
class SpatialOctree {
private:
    static const int MAX_DRAWABLES_PER_NODE = 16;
    static const float MIN_NODE_SIZE = 50.0f; // Minimum size before stopping subdivision
    
    struct OctreeNode;
    OctreeNode* m_root;
    
public:
    SpatialOctree(const Region3D& worldBounds);
    ~SpatialOctree();
    
    void insert(Drawable* draw);
    void remove(Drawable* draw);
    void updatePosition(Drawable* draw, const Coord3D& oldPos);
    void query(const Region3D& region, void (*callback)(Drawable*, void*), void* userData);
    void clear();
};
