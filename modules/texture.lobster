import color

// texture format constants

enum_flags texture_format:
    texture_format_none = 0
    texture_format_clamp = 1
    texture_format_nomipmap
    texture_format_nearest_mag
    texture_format_nearest_min
    texture_format_float
    texture_format_writeonly
    texture_format_readwrite
    texture_format_cubemap
    texture_format_multisample
    texture_format_single_channel
    texture_format_3d
    texture_format_buffer_has_mips
    texture_format_depth

// blending constants

enum blend_mode:
    blend_none
    blend_alpha  // Default.
    blend_add
    blend_add_alpha
    blend_multiply
    blend_pre_multiplied_alpha

// FIXME: put this in a generic map type.
private let texture_names:[string] = []
private let textures:[resource] = []

def loadtexturecached(name, tf:texture_format):
    let n, i = binary_search(texture_names, name)
    if n:
        return textures[i]
    else:
        let tex = gl_load_texture(name, tf)
        if tex:
            texture_names.insert(i, name)
            textures.insert(i, tex)
            return tex
        else:
            print "unable to load texture: " + name
            return from program

def loadtexturecached(name): return loadtexturecached(name, texture_format_none)  // FIXME: default args.

// Pass nil for tex to have it created. Recreates tex if the wrong size. Renders f. Returns tex.
// Depth texture automatically created if depth is true and depthtex is nil.
def render_to_texture(tex, size, depth, depthtex, texture_format, f):
    if !tex || gl_texture_size(tex) != size:
        tex = gl_create_blank_texture(size, color_clear, texture_format)
    assert tex  // FIXME
    assert gl_switch_to_framebuffer(tex, depth, texture_format, nil, depthtex)
    f()
    assert gl_switch_to_framebuffer(nil)
    return tex

def create_depth_texture(tex, size):
    if !tex || gl_texture_size(tex) != size:
        tex = gl_create_blank_texture(size, color_black, texture_format_depth |
            texture_format_nomipmap | texture_format_clamp | texture_format_nearest_mag)
    assert tex  // FIXME
    return tex

// Same, but processes a source texture. Takes size from that texture.
def render_to_texture_from(tex, shader, srctex):
    let texture_format = texture_format_clamp | texture_format_nomipmap | texture_format_nearest_mag
    return render_to_texture(tex, gl_texture_size(srctex), false, nil, texture_format):
        gl_set_primitive_texture(0, srctex)
        gl_set_shader(shader)
        gl_rect(float(gl_texture_size(srctex)))

// Same but renders to the screen.
def post_process(tex, size, shader, depth, f):
    let texture_format = texture_format_clamp | texture_format_nomipmap
    tex = render_to_texture(tex, size, depth, nil, texture_format, f)
    gl_set_primitive_texture(0, tex, texture_format)
    gl_set_shader(shader)
    gl_translate(xy_f { 0, size.y }):
        gl_rect(xy_f { size.x, -size.y })
    return tex
