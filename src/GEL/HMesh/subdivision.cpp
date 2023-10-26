/* ----------------------------------------------------------------------- *
 * This file is part of GEL, http://www.imm.dtu.dk/GEL
 * Copyright (C) the authors and DTU Informatics
 * For license and list of authors, see ../../doc/intro.pdf
 * ----------------------------------------------------------------------- */

#include <GEL/HMesh/dual.h>
#include <GEL/HMesh/subdivision.h>

#include <vector>
#include <GEL/CGLA/Vec3d.h>

#include <GEL/HMesh/Manifold.h>
#include <GEL/HMesh/AttributeVector.h>

namespace HMesh
{
    using namespace std;
    using namespace CGLA;
    
    void loop_split(Manifold& m_in, Manifold& m)
    {
        if(&m != &m_in)
            m = m_in;
        VertexAttributeVector<int> vtouched(m.allocated_vertices(), 0);
        
        vector<HalfEdgeID> hedges;
        for(HalfEdgeID h : m.halfedges())
            if(h<m.walker(h).opp().halfedge())
                hedges.push_back(h);
        
        for(HalfEdgeID h : hedges)
            vtouched[m.split_edge(h)] = 1;
        
        FaceSet faces(m.faces());
        for(FaceID fid : faces)
        {
            Walker w = m.walker(fid);
            
            if(vtouched[w.vertex()] == 0)
                w = w.next();
            
            assert(vtouched[w.vertex()] == 1);
            
            VertexID v1, orig_vert = w.vertex();
            w = w.next();
            FaceID f = fid;
            do
            {
                VertexID v0 = w.opp().vertex();
                w = w.next();
                v1  = w.vertex();
                w = w.next();
                assert(vtouched[v0] == 1);
                assert(vtouched[v1] == 1);
                f = m.split_face_by_edge(f, v0, v1);
            }
            while (v1 != orig_vert);
        }
    }
    
    void cc_split(Manifold& m_in, Manifold& m_out)
    {
        const int Invalid = -1;

        vector<Vec3d> new_points;
        new_points.reserve(m_in.no_vertices());

        VertexAttributeVector<int> vtouched(m_in.allocated_vertices(), Invalid);
        HalfEdgeAttributeVector<int> htouched(m_in.allocated_halfedges(), Invalid);

        int npsize = 0;
        for(VertexIDIterator v = m_in.vertices_begin(); v != m_in.vertices_end(); ++v){       
            vtouched[*v] = npsize;
            new_points.push_back(m_in.pos(*v));
            ++npsize;
        }

        for(HalfEdgeIDIterator h = m_in.halfedges_begin(); h != m_in.halfedges_end(); ++h){
            if(htouched[*h] != Invalid)
                continue;

            Walker w = m_in.walker(*h);
            htouched[*h] = htouched[w.opp().halfedge()] = npsize;
            new_points.push_back((m_in.pos(w.vertex()) + m_in.pos(w.opp().vertex())) * 0.5f);
            ++npsize;

        }
        vector<int> indices;
        vector<int> faces;

        for(FaceIDIterator f = m_in.faces_begin(); f != m_in.faces_end(); ++f){           
            for(Walker w = m_in.walker(*f); !w.full_circle(); w = w.circulate_face_cw()){
                indices.push_back(npsize);
                indices.push_back(htouched[w.halfedge()]);
                indices.push_back(vtouched[w.vertex()]);
                indices.push_back(htouched[w.next().halfedge()]);
                faces.push_back(4);
            }
            new_points.push_back(centre(m_in, *f));
            ++npsize;
        }

        m_out.clear();
        build(m_out, npsize, reinterpret_cast<double*>(&new_points[0]), faces.size(), &faces[0], &indices[0]);
    }
    
    void root3_subdivide(Manifold& m_in, Manifold& m)
    {
        if(&m != &m_in)
            m = m_in;
        
        VertexAttributeVector<int> vtouched(m.allocated_vertices(), 0);
        VertexAttributeVector<Vec3d> new_pos(m.allocated_vertices(), Vec3d(0));
        
        for (VertexIDIterator vid = m.vertices_begin(); vid != m.vertices_end(); ++vid) {
            int v = valency(m, *vid);
            double beta = (4.0 - 2.0 * cos(2.0 * M_PI / v))/(9.0*v);
            new_pos[*vid] = (1.0 - v * beta) * m.pos(*vid);
            for(Walker w = m.walker(*vid); !w.full_circle(); w = w.circulate_vertex_ccw())
            {
                new_pos[*vid] += beta * m.pos(w.vertex());
            }
        }

        vector<FaceID> faces;
        for(FaceIDIterator f = m.faces_begin(); f != m.faces_end(); ++f)
            faces.push_back(*f);
        for(int i=0;i<faces.size(); ++i)
            vtouched[m.split_face_by_vertex(faces[i])] = 1;
        
        for(HalfEdgeIDIterator h = m.halfedges_begin(); h != m.halfedges_end(); ++h)
        {
            Walker w = m.walker(*h);
            
            if(vtouched[w.vertex()] == 0 && vtouched[w.opp().vertex()] == 0 &&
                precond_flip_edge(m, *h))
                m.flip_edge(*h);
        }
        
        for (VertexIDIterator vid = m.vertices_begin(); vid != m.vertices_end(); ++vid)
            if(vtouched[*vid] == 0)
                m.pos(*vid) = new_pos[*vid];
    }

    void rootCC_subdivide(Manifold& m_in, Manifold& m)
    {
        if(&m != &m_in)
            m = m_in;
        
        VertexAttributeVector<int> vtouched(m.allocated_vertices(), 0);
        VertexAttributeVector<Vec3d> new_pos(m.allocated_vertices(), Vec3d(0));
        
        for (VertexIDIterator vid = m.vertices_begin(); vid != m.vertices_end(); ++vid) {
            int v = valency(m, *vid);
            double beta = (4.0 - 2.0 * cos(2.0 * M_PI / v))/(9.0*v);
            new_pos[*vid] = (1.0 - v * beta) * m.pos(*vid);
            for(Walker w = m.walker(*vid); !w.full_circle(); w = w.circulate_vertex_ccw())
            {
                new_pos[*vid] += beta * m.pos(w.vertex());
            }
        }
        
        vector<FaceID> faces;
        for(FaceIDIterator f = m.faces_begin(); f != m.faces_end(); ++f)
            faces.push_back(*f);
        for(int i=0;i<faces.size(); ++i)
            vtouched[m.split_face_by_vertex(faces[i])] = 1;
        
        for(HalfEdgeIDIterator h = m.halfedges_begin(); h != m.halfedges_end(); ++h)
        {
            Walker w = m.walker(*h);
            
            if(vtouched[w.vertex()] == 0 && vtouched[w.opp().vertex()] == 0)
                m.merge_faces(w.face(), *h);
        }
        
        for (VertexIDIterator vid = m.vertices_begin(); vid != m.vertices_end(); ++vid)
            if(vtouched[*vid] == 0)
                m.pos(*vid) = new_pos[*vid];
    }

    
    void butterfly_subdivide(Manifold& m_in, Manifold& m)
    {
        const float S[4][6] = {{5.0/12.0, -1.0/12.0, -1.0/12.0, 0, 0, 0},
            {3.0/8.0, 0, -1.0/8.0, 0, 0, 0},
            {0.35,
                0.03090169943749475,
                -0.08090169943749474,
                -0.08090169943749474,
                0.03090169943749468,0 },
            {0, 1.0f/8, -1.0f/8, 0, -1.0f/8, 1.0f/8}};

        
        if(&m != &m_in)
            m = m_in;

        HalfEdgeAttributeVector<Vec3d> new_vertices_pos(m.allocated_halfedges(), Vec3d(0.0));
        HalfEdgeAttributeVector<int> htouched(m.allocated_halfedges(), 0);
        VertexAttributeVector<int> vtouched(m.allocated_vertices(), 0);

        for(HalfEdgeIDIterator hid = m.halfedges_begin(); hid != m.halfedges_end(); ++hid)
            {
                Walker w = m.walker(*hid);
                VertexID v0 = w.opp().vertex();
                
                int K = valency(m, v0);
                int Ko = valency(m, w.vertex());
                if((K==6 && Ko==6)|| K != 6)
                {
                    Vec3d pos((K==6 ? 1.0 : 3.0/4.0) * m.pos(v0));
                    for(int k=0;k<K; ++k, w = w.circulate_vertex_ccw())
                    {
                        double s = (K<=6) ? S[K-3][k]:(0.25+cos((2.0*M_PI*k)/K)+0.5*cos((4.0*M_PI*k)/K))/K;
                        pos += s * m.pos(w.vertex());                        
                    }
                    new_vertices_pos[*hid] = pos;
                    htouched[*hid] = 1;
                }
            }
        loop_split(m, m);

        for(HalfEdgeIDIterator hid = m.halfedges_begin(); hid != m.halfedges_end(); ++hid)
            if(htouched[*hid])
            {
                VertexID v = m.walker(*hid).opp().vertex();
                if(vtouched[v] == 0)
                    m.pos(v) =Vec3d(0);
                m.pos(v) += new_vertices_pos[*hid];
                vtouched[v]+=1;
            }
        for(VertexIDIterator vid = m.vertices_begin(); vid != m.vertices_end(); ++vid)
            if(vtouched[*vid])
                m.pos(*vid) /= vtouched[*vid];
    }
    
    enum Subd {QUAD_SUBD, CC_SUBD, LOOP_SUBD, TRI_SUBD};
    
    void subd_smooth(Subd subd_method, Manifold& m)
    {
        VertexAttributeVector<Vec3d> new_vertices(Vec3d(0));
        
        for(auto f: m.faces())
        {
            circulate_face_ccw(m, f, [&](VertexID v0) {
                double val = valency(m, v0);
                double A,B;
                
                switch(subd_method)
                {
                    case QUAD_SUBD:
                        A = 1.0 / (4.0 * val);
                        B = 1.0 / (4.0 * val);
                        break;
                    case CC_SUBD:
                        A = (1.0-3.0/val) * (1.0/val);
                        B = sqr(1.0/val);
                        break;
                    case TRI_SUBD:
                        A = 2.0 / (8.0 * val);
                        B = 3.0 / (8.0 * val);
                        break;
                    case LOOP_SUBD:
                        float w = 5.0/8.0 - sqr(3.0/8.0 + 0.25 * cos(2.0*M_PI/val));
                        A = (1.0-2.0*w)/val;
                        B = w/val;
                        break;
                }
                circulate_face_ccw(m, f, [&](VertexID v) {
                    if(v == v0)
                        new_vertices[v0] += A * m.pos(v);
                    else
                        new_vertices[v0] += B * m.pos(v);
                });
                
            });
        }
        m.positions_attribute_vector() = new_vertices;
    }

    void cc_smooth(Manifold& m)
    {
        subd_smooth(CC_SUBD, m);
    }

    void volume_preserving_cc_smooth(Manifold& m, int iter)
    {
        for (int i=0;i<iter;++i) {
        subd_smooth(CC_SUBD, m);
        auto old_pos = m.positions_attribute_vector();
        subd_smooth(CC_SUBD, m);
        for (auto v: m.vertices())
            m.pos(v) = old_pos[v] - 0.98 *(m.pos(v) - old_pos[v]);
        }
    }

    
    void loop_smooth(Manifold& m)
    {
        subd_smooth(LOOP_SUBD, m);
    }


}
