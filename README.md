### UMESH TOOLS

A collection of tools for wrangling James Schmaltz's good ol' UE1 Vertex Mesh
format.

- **umodelinfo** : Infodump on an anivfile/datafile pair.
- **datatrim** : Trim triangles off a datafile based on uvs and tex numbers.
- **anivtrim** : Discards a range of frames out of an anivfile.
- **umodeldiscard** : Discards vertices from an anivfile that are unreferenced
  in the datafile (indices will be remapped).
- **dxconv** : Converts anivfile back and forth between the original packed
  XY11Z10 and the Deus Ex padded XYZ16 vertex formats.
- **vertdup** : [TODO] Lists groups of vertices that are identical.
  (same position in all frames, optionally also same uvs in all referenced
  triangles and same texture numbers)
- **vertdedup** : [TODO] Merges identical vertices. Note that this may cause
  undesired results. vertmerge is available as a manual alternative.
- **vertmerge** : [TODO] Merges all specified vertices together as long as they
  have the same position in all frames.
- **umeshview** : [TODO] Small SDL2 program for displaying models. WASD for
  movement, Q and E to move up and down, arrow keys for rotation, page up and
  page down to roll, home to reset animation parameters, end to reset camera
  parameters. insert and delete to control animation speed, space to pause,
  enter and backspace to step one frame. Shift can be used as a speed modifier,
  for some of the movements. Texture groups are color coded, unreferenced
  vertices show as red squares. Normally you're required to pass the origin,
  scale and rotation parameters used for mesh import, as the default transforms
  may not give the best result. Textures can be loaded and assigned from the
  command line too, with a variety of supported formats, including PCX files
  directly exported from UnrealEd. Requires SDL2 (obviously), OpenGL (also
  obviously), libepoxy and SDL2_image.
- **umesh** : [TODO] The big boy, the visual mesh editor that I initially
  planned to make. Once this one is done, all the other tools will be obsolete.
- **umodelextract** : A horrible abomination of C code that extracts meshes
  directly from UE packages, without any form of data mangling, unlike Umodel,
  or corruption, unlike UTPT. Each mesh/lodmesh found will be exported as raw
  object data (if you want to inspect the full thing), a plaintext
  representation of the Mesh and LodMesh structures in-package (absolute jumps
  are converted to relative offsets), and most importantly, a generated
  anivfile and datafile pair, plus a script file for re-importing the model.
  (note that due to mysterious Tim Sweeney black magic, there is no way to
  tell if a model has been imported with UNMIRROR=1, so this will be missing
  from the generated .uc file).
- **texnumsq** : Sometimes models have "scattered" texture numbers (i.e.:
  texture indices don't start at zero, or there are "jumps" between used
  indices). This tool remaps all texture indices to the lowest unused index,
  effectively "squashing" them together and removing gaps.
- **polymangler** : [TODO] A tool that replicates the "mangling" Umodel does on
  mesh export, where it effectively ignores LodMesh materials and instead comes
  up with its own indices. Included for backwards compatibility with stuff that
  expects this sort of nonsense.
- **umesh2obj** : Creates .obj files for each frame of a mesh.
- **attacher** : Attaches one mesh to the weapon triangle of another.
- **unmirror** : Flips the model on the X axis, for models that are usually
  imported with the UNMIRROR flag in UE1 (e.g.: player models).
- **polyflip** : Flips a specified set of polys in one datafile (by swapping
  vertices 1 and 2 in them). Can be used to fix silly mistakes that go
  unnoticed if, for example, the mesh is edited without backface culling on.
- **polysort** : Rearranges polys in a datafile for optimal loading and
  drawing. Can help fix transparency ordering issues in gzdoom.
- **setumeshflag** : Sets/unsets poly type data (render styles, hints, etc.).
