set(WW3D2_SRC
    aabtree.cpp
    aabtree.h
    #aabtreebuilder.cpp
    #aabtreebuilder.h
    agg_def.cpp
    agg_def.h
    animatedsoundmgr.cpp
    animatedsoundmgr.h
    #animobj.cpp
    #animobj.h
    #assetmgr.cpp
    #assetmgr.h
    assetstatus.cpp
    assetstatus.h
    bitmaphandler.cpp
    bitmaphandler.h
    bmp2d.cpp
    bmp2d.h
    #boxrobj.cpp
    #boxrobj.h
    bwrender.cpp
    bwrender.h
    #camera.cpp
    #camera.h
    #classid.h # unused
    collect.cpp
    collect.h
    colorspace.h
    coltest.cpp
    coltest.h
    coltype.h
    composite.cpp
    composite.h
    #dazzle.cpp
    #dazzle.h
    #ddsfile.cpp
    #ddsfile.h
    #decalmsh.cpp
    #decalmsh.h
    decalsys.cpp
    decalsys.h
    distlod.cpp
    distlod.h
    dllist.h
    DX8Backend.cpp
    DX8Backend.h
    dx8caps.cpp
    dx8caps.h
    #dx8fvf.cpp
    #dx8fvf.h
    #dx8indexbuffer.cpp
    #dx8indexbuffer.h
    dx8list.h
    dx8polygonrenderer.cpp
    dx8polygonrenderer.h
    #dx8renderer.cpp
    #dx8renderer.h
    dx8rendererdebugger.cpp
    dx8rendererdebugger.h
    dx8texman.cpp
    dx8texman.h
    #dx8vertexbuffer.cpp
    #dx8vertexbuffer.h
    dx8webbrowser.cpp
    dx8webbrowser.h
    dx8wrapper.cpp
    dx8wrapper.h
    dynamesh.cpp
    dynamesh.h
    font3d.cpp
    font3d.h
    formconv.cpp
    formconv.h
    FramGrab.cpp
    framgrab.h
    hanim.cpp
    hanim.h
    #hanimmgr.cpp
    #hanimmgr.h
    hcanim.cpp
    hcanim.h
    #hlod.cpp
    #hlod.h
    hmdldef.cpp
    hmdldef.h
    #hmorphanim.cpp
    #hmorphanim.h
    #hrawanim.cpp
    #hrawanim.h
    htree.cpp
    htree.h
    #htreemgr.cpp
    #htreemgr.h
    IRenderBackend.h
    intersec.cpp
    intersec.h
    intersec.inl
    inttest.h
    layer.cpp
    layer.h
    #light.cpp
    #light.h
    #lightenvironment.cpp
    #lightenvironment.h
    line3d.cpp
    line3d.h
    #linegrp.cpp
    #linegrp.h
    #mapper.cpp
    #mapper.h
    matinfo.cpp
    matinfo.h
    matpass.cpp
    matpass.h
    #matrixmapper.cpp
    #matrixmapper.h
    #mesh.cpp
    #mesh.h
    #meshbuild.cpp
    #meshbuild.h
    meshdam.cpp
    meshdam.h
    #meshgeometry.cpp
    #meshgeometry.h
    #meshmatdesc.cpp
    #meshmatdesc.h
    #meshmdl.cpp
    #meshmdl.h
    #meshmdlio.cpp
    metalmap.cpp
    metalmap.h
    missingtexture.cpp
    missingtexture.h
    #motchan.cpp
    #motchan.h
    #nullrobj.cpp
    #nullrobj.h
    #part_buf.cpp
    #part_buf.h
    #part_emt.cpp
    #part_emt.h
    #part_ldr.cpp
    #part_ldr.h
    pivot.cpp
    pivot.h
    pointgr.cpp
    pointgr.h
    polyinfo.cpp
    polyinfo.h
    predlod.cpp
    predlod.h
    prim_anim.cpp
    prim_anim.h
    projector.cpp
    projector.h
    proto.cpp
    proto.h
    proxy.h
    rddesc.h
    RenderBackend.cpp
    RenderBackend.h
    #render2d.cpp
    #render2d.h
    render2dsentence.cpp
    render2dsentence.h
    renderobjectrecycler.cpp
    renderobjectrecycler.h
    rendobj.cpp
    rendobj.h
    #rinfo.cpp
    #rinfo.h
    ringobj.cpp
    ringobj.h
    robjlist.h
    #scene.cpp
    #scene.h
    segline.cpp
    segline.h
    seglinerenderer.cpp
    seglinerenderer.h
    #shader.cpp
    #shader.h
    shattersystem.cpp
    shattersystem.h
    shdlib.h
    snappts.cpp
    snapPts.h
    #sortingrenderer.cpp
    #sortingrenderer.h
    soundlibrarybridge.h
    soundrobj.cpp
    soundrobj.h
    sphereobj.cpp
    sphereobj.h
    static_sort_list.cpp
    static_sort_list.h
    statistics.cpp
    statistics.h
    streak.cpp
    streak.h
    streakRender.cpp
    streakRender.h
    stripoptimizer.cpp
    stripoptimizer.h
    surfaceclass.cpp
    surfaceclass.h
    texproject.cpp
    texproject.h
    #textdraw.cpp # unused
    textdraw.h
    texture.cpp
    texture.h
    texturefilter.cpp
    texturefilter.h
    textureloader.cpp
    textureloader.h
    texturethumbnail.cpp
    texturethumbnail.h
    #vertmaterial.cpp
    #vertmaterial.h
    visrasterizer.cpp
    visrasterizer.h
    w3d_dep.cpp
    w3d_dep.h
    #w3d_file.h
    w3d_obsolete.h
    w3d_util.cpp
    w3d_util.h
    w3derr.h
    w3dexclusionlist.cpp
    w3dexclusionlist.h
    #ww3d.cpp
    #ww3d.h
    ww3dformat.cpp
    ww3dformat.h
    ww3dids.h
    ww3dtrig.h
)

add_library(corei_ww3d2 INTERFACE)

target_sources(corei_ww3d2 INTERFACE ${WW3D2_SRC})

# TheSuperHackers @refactor bobtista 10/04/2026 Conditionally include the
# active render backend's source files. See cmake/render-backend.cmake and
# Core/Libraries/Source/WWVegas/WW3D2/PHASE2.md.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_sources(corei_ww3d2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/BgfxBackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/BgfxBackend.h
    )

    # TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.1 shader
    # compilation. Only compile these shaders for the bgfx build. The
    # ggc_compile_bgfx_shader helper (cmake/bgfx.cmake) creates the
    # ggc_bgfx_shaders STATIC library on first call and attaches every
    # generated .bin.h to it. corei_ww3d2 links it INTERFACE so g_ww3d2 /
    # z_ww3d2 inherit the include directory and build order dependency.
    ggc_compile_bgfx_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_passthrough.sc)
    ggc_compile_bgfx_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_passthrough.sc)
    # TheSuperHackers @refactor bobtista 12/04/2026 Phase 5A uber shader
    # pair. Single program handles all TSS combinations via uniforms.
    # Replaces the Phase 4D.1 per-preset shader pairs.
    ggc_compile_bgfx_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_uber.sc)
    ggc_compile_bgfx_shader(${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_uber.sc)
    target_link_libraries(corei_ww3d2 INTERFACE ggc_bgfx_shaders)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    target_sources(corei_ww3d2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/DiligentBackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DiligentBackend.h
    )
endif()

# Propagate the active backend's compile define to every translation unit
# that pulls in WW3D2, so that RenderBackend.cpp and any future callers can
# #if on GGC_RENDER_BACKEND_DX8 / _BGFX / _DILIGENT.
if(DEFINED GGC_RENDER_BACKEND_COMPILE_DEFINE)
    target_compile_definitions(corei_ww3d2 INTERFACE ${GGC_RENDER_BACKEND_COMPILE_DEFINE})
endif()

if (MSVC AND NOT IS_VS6_BUILD)
    target_link_libraries(corei_ww3d2 INTERFACE
        comsuppw
    )
endif()

target_link_libraries(corei_ww3d2 INTERFACE
    core_browserengine
    core_wwlib
    core_wwmath
)

# Link the backend's native libraries into whoever consumes corei_ww3d2.
if(GGC_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(corei_ww3d2 INTERFACE bgfx bx bimg)
elseif(GGC_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(corei_ww3d2 INTERFACE
        Diligent-GraphicsEngineD3D11-static
        Diligent-Common
        Diligent-GraphicsTools
    )
endif()
