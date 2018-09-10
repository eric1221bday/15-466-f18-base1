#!/usr/bin/env python

# based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

# Note: Script meant to be executed from within blender, as per:
# blender --background --python export-scene.py -- <infile.blend> <layer> <outfile.scene>

import sys, re

args = []
for i in range(0, len(sys.argv)):
    if sys.argv[i] == '--':
        args = sys.argv[i + 1:]

if len(args) != 2:
    print(
        "\n\nUsage:\nblender --background --python export-scene.py -- <infile.blend>[:layer] <outfile.scene>\nExports the walk mech in layer (default 3) to a binary blob\n")
    exit(1)

infile = args[0]
layer = 3
m = re.match(r'^(.*):(\d+)$', infile)
if m:
    infile = m.group(1)
    layer = int(m.group(2))
outfile = args[1]

print("Will export walk mesh from layer " + str(layer) + " from file '" + infile + "' to blob '" + outfile + "'");

import bpy
import mathutils
import struct
import math

# ---------------------------------------------------------------------
# Export scene:

bpy.ops.wm.open_mainfile(filepath=infile)

# Scene file format:
# vtx0 contains all vertices of the walk mesh
# nom0 contains all vertex normals of the walk mesh
# tri0 contains all triangle vertex indices

vertex_data = b""
vertex_normal_data = b""
tri_data = b""

walk_mesh_found = False

for obj in bpy.data.objects:
    if obj.layers[layer - 1] and obj.type == 'MESH' and obj.name == 'WalkMesh':
        walk_mesh_found = True
        for vertex in obj.data.vertices:
            for x in vertex.co:
                vertex_data += struct.pack('f', x)
            for n in vertex.normal:
                vertex_normal_data += struct.pack('f', n)
        for poly in obj.data.polygons:
            # print(poly.vertices[3])
            assert (len(poly.vertices) == 3)
            tri_data += struct.pack('III', poly.vertices[0], poly.vertices[1], poly.vertices[2])

        vertex_count = len(obj.data.vertices)

        # check that we wrote as much data as anticipated:
        assert (vertex_count * 3 * 4 == len(vertex_data))
        assert (len(obj.data.polygons) * 3 * 4 == len(tri_data))
        assert (vertex_count * 3 * 4 == len(vertex_normal_data))

        print(vertex_count, 'number of vertices')
        print(len(obj.data.polygons), 'number of triangles')

if walk_mesh_found:

    # write the strings chunk and scene chunk to an output blob:
    blob = open(outfile, 'wb')


    def write_chunk(magic, data):
        blob.write(struct.pack('4s', magic))  # type
        blob.write(struct.pack('I', len(data)))  # length
        blob.write(data)


    write_chunk(b'vtx0', vertex_data)
    write_chunk(b'tri0', tri_data)
    write_chunk(b'nom0', vertex_normal_data)

    print("Wrote " + str(blob.tell()) + " bytes to '" + outfile + "'")
    blob.close()

else:
    print("Cannot Find Walk Mesh!")
