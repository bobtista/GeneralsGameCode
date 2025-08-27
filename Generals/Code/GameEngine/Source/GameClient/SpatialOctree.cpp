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

#include "../../Include/GameClient/SpatialOctree.h"
#include "../../Include/GameClient/Drawable.h"
#include "../../Include/Common/STLTypedefs.h"

// Helper functions
static bool boundsOverlap(const Region3D& a, const Region3D& b) {
    return (a.lo.x < b.hi.x && a.hi.x > b.lo.x &&
            a.lo.y < b.hi.y && a.hi.y > b.lo.y &&
            a.lo.z < b.hi.z && a.hi.z > b.lo.z);
}

static bool isPointInRegion(const Coord3D& point, const Region3D& region) {
    return (point.x >= region.lo.x && point.x <= region.hi.x &&
            point.y >= region.lo.y && point.y <= region.hi.y &&
            point.z >= region.lo.z && point.z <= region.hi.z);
}

// OctreeNode implementation
struct SpatialOctree::OctreeNode {
    Region3D bounds;
    std::vector<Drawable*> drawables;
    OctreeNode* children[8];
    bool isLeaf;
    
    OctreeNode(const Region3D& nodeBounds) : bounds(nodeBounds), isLeaf(true) {
        for (int i = 0; i < 8; i++) {
            children[i] = NULL;
        }
    }
    
    ~OctreeNode() {
        for (int i = 0; i < 8; i++) {
            delete children[i];
        }
    }
    
    void insert(Drawable* draw);
    void remove(Drawable* draw);
    void query(const Region3D& region, void (*callback)(Drawable*, void*), void* userData);
    void subdivide();
    int getChildIndex(const Coord3D& pos) const;
    bool shouldSubdivide() const;
};

void SpatialOctree::OctreeNode::insert(Drawable* draw) {
    if (isLeaf && drawables.size() < MAX_DRAWABLES_PER_NODE) {
        drawables.push_back(draw);
        return;
    }
    
    if (isLeaf) {
        subdivide();
    }
    
    int childIndex = getChildIndex(*draw->getPosition());
    if (children[childIndex]) {
        children[childIndex]->insert(draw);
    }
}

void SpatialOctree::OctreeNode::remove(Drawable* draw) {
    if (isLeaf) {
        for (auto it = drawables.begin(); it != drawables.end(); ++it) {
            if (*it == draw) {
                drawables.erase(it);
                break;
            }
        }
    } else {
        int childIndex = getChildIndex(*draw->getPosition());
        if (children[childIndex]) {
            children[childIndex]->remove(draw);
        }
    }
}

void SpatialOctree::OctreeNode::query(const Region3D& region, void (*callback)(Drawable*, void*), void* userData) {
    if (!boundsOverlap(bounds, region)) {
        return;
    }
    
    if (isLeaf) {
        for (Drawable* draw : drawables) {
            Coord3D pos = *draw->getPosition();
            if (isPointInRegion(pos, region)) {
                callback(draw, userData);
            }
        }
        return;
    }
    
    for (int i = 0; i < 8; i++) {
        if (children[i]) {
            children[i]->query(region, callback, userData);
        }
    }
}

void SpatialOctree::OctreeNode::subdivide() {
    if (!shouldSubdivide()) {
        return;
    }
    
    float halfWidth = bounds.width() * 0.5f;
    float halfHeight = bounds.height() * 0.5f;
    float halfDepth = bounds.depth() * 0.5f;
    
    float midX = bounds.lo.x + halfWidth;
    float midY = bounds.lo.y + halfHeight;
    float midZ = bounds.lo.z + halfDepth;
    
    // Create 8 child octants
    children[0] = new OctreeNode(Region3D{{bounds.lo.x, bounds.lo.y, bounds.lo.z}, {midX, midY, midZ}});
    children[1] = new OctreeNode(Region3D{{midX, bounds.lo.y, bounds.lo.z}, {bounds.hi.x, midY, midZ}});
    children[2] = new OctreeNode(Region3D{{bounds.lo.x, midY, bounds.lo.z}, {midX, bounds.hi.y, midZ}});
    children[3] = new OctreeNode(Region3D{{midX, midY, bounds.lo.z}, {bounds.hi.x, bounds.hi.y, midZ}});
    children[4] = new OctreeNode(Region3D{{bounds.lo.x, bounds.lo.y, midZ}, {midX, midY, bounds.hi.z}});
    children[5] = new OctreeNode(Region3D{{midX, bounds.lo.y, midZ}, {bounds.hi.x, midY, bounds.hi.z}});
    children[6] = new OctreeNode(Region3D{{bounds.lo.x, midY, midZ}, {midX, bounds.hi.y, bounds.hi.z}});
    children[7] = new OctreeNode(Region3D{{midX, midY, midZ}, {bounds.hi.x, bounds.hi.y, bounds.hi.z}});
    
    // Move existing drawables to appropriate children
    for (Drawable* draw : drawables) {
        int childIndex = getChildIndex(*draw->getPosition());
        children[childIndex]->drawables.push_back(draw);
    }
    
    drawables.clear();
    isLeaf = false;
}

int SpatialOctree::OctreeNode::getChildIndex(const Coord3D& pos) const {
    float midX = bounds.lo.x + bounds.width() * 0.5f;
    float midY = bounds.lo.y + bounds.height() * 0.5f;
    float midZ = bounds.lo.z + bounds.depth() * 0.5f;
    
    int index = 0;
    if (pos.x >= midX) index |= 1;
    if (pos.y >= midY) index |= 2;
    if (pos.z >= midZ) index |= 4;
    
    return index;
}

bool SpatialOctree::OctreeNode::shouldSubdivide() const {
    return bounds.width() > MIN_NODE_SIZE && 
           bounds.height() > MIN_NODE_SIZE && 
           bounds.depth() > MIN_NODE_SIZE;
}

// SpatialOctree implementation
SpatialOctree::SpatialOctree(const Region3D& worldBounds) 
    : m_root(new OctreeNode(worldBounds)) {
}

SpatialOctree::~SpatialOctree() {
    delete m_root;
}

void SpatialOctree::insert(Drawable* draw) {
    if (m_root) {
        m_root->insert(draw);
    }
}

void SpatialOctree::remove(Drawable* draw) {
    if (m_root) {
        m_root->remove(draw);
    }
}

void SpatialOctree::updatePosition(Drawable* draw, const Coord3D& oldPos) {
    // For simplicity, just remove and reinsert
    // In a more optimized version, you'd track the old position
    remove(draw);
    insert(draw);
}

void SpatialOctree::query(const Region3D& region, void (*callback)(Drawable*, void*), void* userData) {
    if (m_root) {
        m_root->query(region, callback, userData);
    }
}

void SpatialOctree::clear() {
    delete m_root;
    m_root = NULL;
}
