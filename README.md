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
- **umeshview** : [TODO] Small SDL2 program for displaying models. Left click
  drag to rotate camera, right click drag to move, mouse wheel for zoom. Page
  up and page down switch between animation frames. Texture groups are color
  coded, unreferenced vertices show as red crosses.
- **umesh** : [TODO] The big boy, the visual mesh editor that I initially
  planned to make. Once this one is done, all the other tools will be obsolete.
