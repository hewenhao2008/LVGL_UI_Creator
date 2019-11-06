/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <idirectfb.h>

#include "X11EGLImpl.h"


D_LOG_DOMAIN( DFBX11_EGLImpl,   "DFBX11/EGL/Impl",   "DirectFB X11 EGL Implementation" );
D_LOG_DOMAIN( DFBX11_EGLConfig, "DFBX11/EGL/Config", "DirectFB X11 EGL Config" );


namespace X11 {

static X11EGLImpl impl;



using namespace DirectFB;

using namespace std::placeholders;


/**********************************************************************************************************************/

//class X11EGLTask : public Graphics::RenderTask
//{
//public:
//     X11EGLTask()
//          :
//          RenderTask( CSAID_NONE )
//     {
//     }
//
//     virtual DFBResult Bind( CoreSurfaceBuffer *draw,
//                             CoreSurfaceBuffer *read )
//     {
//          return DFB_OK;
//     }
//};

/**********************************************************************************************************************/

GLeglImage::GLeglImage( EGL::KHR::Image &egl_image,
                        X11EGLImpl      &impl )
     :
     Type( egl_image ),
     impl( impl ),
     glEGLImage( 0 )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11::GLeglImage::%s( %p, KHR::Image %p, impl %p )\n", __FUNCTION__, this, &egl_image, &impl );

     D_LOG( DFBX11_EGLImpl, VERBOSE, "  -> Deriving GLeglImage for X11 GL Context from generic EGLImageKHR (image %p, surface %p)\n", &egl_image, egl_image.dfb_surface );

     if (impl.eglCreateImageKHR) {
          DFBResult                   ret;
          IDirectFBSurfaceAllocation *allocation;

          ret = egl_image.dfb_surface->GetAllocation( egl_image.dfb_surface, DSBR_FRONT, DSSE_LEFT, "Pixmap/X11", &allocation );
          if (ret) {
               D_DEBUG_AT( DFBX11_EGLImpl, "  -> IDirectFBSurface::GetAllocation( FRONT, LEFT, 'Pixmap/X11' ) failed! (%s)\n", DirectResultString((DirectResult)ret) );

               ret = egl_image.dfb_surface->Allocate( egl_image.dfb_surface, DSBR_FRONT, DSSE_LEFT, "Pixmap/X11", 0, &allocation );
               if (ret) {
                    D_DERROR_AT( DFBX11_EGLImpl, ret, "  -> IDirectFBSurface::Allocate( FRONT, LEFT, 'Pixmap/X11' ) failed!\n" );
                    return;
               }
          }

          u64 handle;

          ret = allocation->GetHandle( allocation, &handle );
          if (ret) {
               D_DERROR_AT( DFBX11_EGLImpl, ret, "  -> IDirectFBSurface::GetAllocation( FRONT, LEFT, 'Pixmap/X11' ) failed!\n" );
               allocation->Release( allocation );
               return;
          }

          allocation->Release( allocation );


          Pixmap pixmap = handle;

          D_DEBUG_AT( DFBX11_EGLImpl, "  -> creating new EGLImage from Pixmap 0x%08lx\n", pixmap );


          glEGLImage = impl.eglCreateImageKHR( impl.egl_display,
                                               EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                               (EGLClientBuffer)(long) pixmap, NULL );
          if (!glEGLImage) {
               D_ERROR_AT( DFBX11_EGLImpl, "  -> creating new EGLImage from Pixmap 0x%08lx failed (%s)\n",
                           pixmap, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ) );
               return;
          }

          D_LOG( DFBX11_EGLImpl, VERBOSE, "  => New EGLImage 0x%08lx from Pixmap 0x%08lx\n", (long) glEGLImage, pixmap );
     }
}

GLeglImage::~GLeglImage()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "RCar::GLeglImage::%s( %p )\n", __FUNCTION__, this );

     if (glEGLImage)
          impl.eglDestroyImageKHR( impl.egl_display, glEGLImage );
}

/**********************************************************************************************************************/

X11Display::X11Display( ::Display *x11_display )
     :
     x11_display( x11_display )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11Display::%s( %p )\n", __FUNCTION__, this );

}

X11Display::~X11Display()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11Display::%s( %p )\n", __FUNCTION__, this );
}

void
X11Display::Sync( bool always )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11Display::%s( %p )\n", __FUNCTION__, this );

     if (always) {
          XLockDisplay( x11_display );
          XSync( x11_display, False );
          XUnlockDisplay( x11_display );
     }
}

void
X11DisplaySync::Sync( bool always )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11DisplaySync::%s( %p )\n", __FUNCTION__, this );

     X11Display::Sync( true );
}

/**********************************************************************************************************************/

void
X11EGLImpl::Context_glEGLImageTargetTexture2D( GL::enum_  &target,
                                               GLeglImage &image )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLImpl::Context_glEGLImageTargetTexture2D::%s( %p, target 0x%04x, X11::GLeglImage %p, EGLImage %p )\n",
            __FUNCTION__, this, target, &image, image.glEGLImage );

     if (!image.glEGLImage) {
          void                  *data;
          int                    pitch;
          int                    width;
          int                    height;
          DFBSurfacePixelFormat  format;

          image.parent.dfb_surface->GetPixelFormat( image.parent.dfb_surface, &format );

          image.parent.dfb_surface->GetSize( image.parent.dfb_surface, &width, &height );

          image.parent.dfb_surface->Lock( image.parent.dfb_surface, DSLF_READ, &data, &pitch );

          D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling glTexImage2D( target 0x%04x, data %p )...\n", target, data );
          D_INFO( "  -> calling glTexImage2D( target 0x%04x, %dx%d, data %p )...\n", target, width, height, data );

          glPixelStorei( GL_UNPACK_ROW_LENGTH_EXT, pitch/4 );

          if (format == DSPF_ABGR) {
               glTexImage2D( target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
          }
          else {
               glTexImage2D( target, 0, GL_BGRA_EXT, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data );
          }

          image.parent.dfb_surface->Unlock( image.parent.dfb_surface );
     }
     else
          glEGLImageTargetTexture2DOES( target, image.glEGLImage );
}

void *
X11EGLImpl::Context_eglGetProcAddress( const char *name )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLImpl::%s( %p, name '%s' )\n", __FUNCTION__, this, name );

     return (void*) impl.lib.eglGetProcAddress( name );
}


/**********************************************************************************************************************/

X11EGLImpl::X11EGLImpl()
     :
     eglCreateImageKHR( NULL ),
     eglDestroyImageKHR( NULL ),
     glEGLImageTargetTexture2DOES( NULL ),
     egl_display( EGL_NO_DISPLAY ),
     egl_major( 0 ),
     egl_minor( 0 )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLImpl::%s( %p )\n", __FUNCTION__, this );

     //X11::GLeglImage::Register<
     //     std::function< X11::GLeglImage * ( Types::TypeBase *source ) >
     //     >( DirectFB::EGL::KHR::Image::GetTypeInstance().GetInfo().name,
     //        std::bind( []( Types::TypeBase *source,
     //                       X11EGLImpl      &impl )
     //                    {
     //                         return new X11::GLeglImage( (DirectFB::EGL::KHR::Image&) *source, impl );
     //                    },
     //std::placeholders::_1, std::ref(*this) ) );

     X11::GLeglImage::RegisterConversion< DirectFB::EGL::KHR::Image, X11EGLImpl& >( *this );

     Graphics::Context::Register< GL::OES::glEGLImageTargetTexture2D >( "",
                                                                        std::bind( &X11EGLImpl::Context_glEGLImageTargetTexture2D, this, _1, _2 ) );

     EGL::Core::Register< EGL::Core::GetProcAddress >( "",
                                                       std::bind( &X11EGLImpl::Context_eglGetProcAddress, this, _1 ) );

     Register();
}

X11EGLImpl::~X11EGLImpl()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLImpl::%s( %p )\n", __FUNCTION__, this );
}

const Direct::String &
X11EGLImpl::GetName() const
{
     static Direct::String name( "X11EGL" );

     return name;
}

DirectResult
X11EGLImpl::Initialise()
{
     DFBResult   ret;
     const char *client_apis;
     const char *egl_vendor;
     const char *egl_extensions;
     EGLConfig  *egl_configs;
     EGLint      num_configs = 0;

     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLImpl::%s( %p )\n", __FUNCTION__, this );

     x11_display = XOpenDisplay( getenv("DISPLAY") );

     if (direct_config_get_int_value( "x11-sync" ))
          display = new X11DisplaySync( x11_display );
     else
          display = new X11Display( x11_display );

     ret = lib.Init( "/usr/lib/x86_64-linux-gnu/mesa-egl/libEGL.so.1", true, false );
     if (ret) {
          ret = lib.Init( "/usr/lib/i386-linux-gnu/mesa-egl/libEGL.so.1", true, false );
          if (ret)
               return (DirectResult) ret;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglGetDisplay( %p )\n", x11_display );

     char *old_platform = getenv( "EGL_PLATFORM" );

     setenv( "EGL_PLATFORM", "x11", 1 );

     egl_display = lib.eglGetDisplay( (EGLNativeDisplayType) x11_display );
     if (!egl_display) {
          D_ERROR( "X11/EGLImpl: eglGetDisplay() failed\n" );
          return (DirectResult) DFB_FAILURE;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  => EGLDisplay %p\n", egl_display );

     ret = lib.Load();
     if (ret)
          return (DirectResult) ret;

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglInitialize()\n" );

     if (!lib.eglInitialize( egl_display, &egl_major, &egl_minor )) {
          D_ERROR( "X11/EGLImpl: eglInitialize() failed\n" );
          goto failure;
     }

     if (old_platform)
          setenv( "EGL_PLATFORM", old_platform, 1 );
     else
          unsetenv( "EGL_PLATFORM" );

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> version %d.%d\n", egl_major, egl_minor );

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglQueryString:%p\n",
                 lib.eglQueryString );


     egl_vendor = lib.eglQueryString( egl_display, EGL_VENDOR );

     if (!egl_vendor) {
          D_ERROR( "X11/EGLImpl: eglQueryString( %p, EGL_VENDOR ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> vendor '%s'\n", egl_vendor );


     client_apis = lib.eglQueryString( egl_display, EGL_CLIENT_APIS );

     if (!client_apis) {
          D_ERROR( "X11/EGLImpl: eglQueryString( %p, EGL_CLIENT_APIS ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> apis '%s'\n", client_apis );


     egl_extensions = lib.eglQueryString( egl_display, EGL_EXTENSIONS );

     if (!egl_extensions) {
          D_ERROR( "X11/EGLImpl: eglQueryString( %p, EGL_EXTENSIONS ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> extensions '%s'\n", egl_extensions );


     name       = Direct::String::F( "X11 EGL (on %s)", egl_vendor );
     apis       = Direct::String( client_apis ).GetTokens( " " );
     //extensions = Direct::String( egl_extensions ).GetTokens( " " );


     if (!lib.eglGetConfigs( egl_display, NULL, 0, &num_configs )) {
          D_ERROR( "X11/EGLImpl: eglGetConfigs( %p, 0 ) failed\n", egl_display );
          goto failure;
     }

     egl_configs = (EGLConfig*) D_MALLOC( num_configs * sizeof(EGLConfig) );

     if (!lib.eglGetConfigs( egl_display, egl_configs, num_configs, &num_configs )) {
          D_ERROR( "X11/EGLImpl: eglGetConfigs( %p ) failed\n", egl_display );
          D_FREE( egl_configs );
          goto failure;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> got %u configs\n", num_configs );

     for (EGLint i=0; i<num_configs; i++)
          configs.push_back( new X11EGLConfig( *this, egl_configs[i] ) );

     D_FREE( egl_configs );


     if (strstr( egl_extensions, "EGL_KHR_image_pixmap" )) {
          eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) lib.Lookup( "eglCreateImageKHR" );
          D_DEBUG_AT( DFBX11_EGLImpl, "  -> eglCreateImageKHR = %p\n", eglCreateImageKHR );

          eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) lib.Lookup( "eglDestroyImageKHR" );
          D_DEBUG_AT( DFBX11_EGLImpl, "  -> eglDestroyImageKHR = %p\n", eglDestroyImageKHR );

          glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) lib.Lookup( "glEGLImageTargetTexture2DOES" );
          D_DEBUG_AT( DFBX11_EGLImpl, "  -> glEGLImageTargetTexture2DOES = %p\n", glEGLImageTargetTexture2DOES );

          D_INFO( "X11/EGL: Using EGLImage extension\n" );
     }


     core->RegisterImplementation( this );

     return DR_OK;


failure:
     lib.eglTerminate( egl_display );
     lib.Unload();
     egl_display = NULL;
     return DR_FAILURE;
}

DirectResult
X11EGLImpl::Finalise()
{
     core->UnregisterImplementation( this );

     for (auto it=configs.begin(); it!=configs.end(); it++)
          delete *it;

     if (egl_display)
          lib.eglTerminate( egl_display );

     delete display;

     XCloseDisplay( x11_display );

     return DR_OK;
}

/**********************************************************************************************************************/

X11EGLConfig::X11EGLConfig( X11EGLImpl &impl,
                            EGLConfig   egl_config )
     :
     Config( &impl ),
     impl( impl ),
     egl_config( egl_config )
{
     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p, egl_config 0x%08lx )\n", __FUNCTION__, this, (long) egl_config );

}

X11EGLConfig::~X11EGLConfig()
{
     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p )\n", __FUNCTION__, this );

}

DFBResult
X11EGLConfig::GetOption( const Direct::String &name,
                         long                 &value )
{
     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p, '%s' ) <- egl_config 0x%08lx\n", __FUNCTION__, this, *name, (long) egl_config );

     DirectFB::EGL::EGLInt option;

     if (FromString<DirectFB::EGL::EGLInt>( option, name )) {
          D_DEBUG_AT( DFBX11_EGLConfig, "  => 0x%04x\n", option.value );

          EGLint v;

          if (!impl.lib.eglGetConfigAttrib( impl.egl_display, egl_config, option.value, &v)) {
               D_ERROR( "X11/EGLConfig: eglGetConfigAttrib( %p, config %p, 0x%04x '%s' ) failed\n",
                        impl.egl_display, (void*) (long) egl_config, option.value, *name );
               return DFB_FAILURE;
          }

          D_DEBUG_AT( DFBX11_EGLConfig, "  => 0x%x\n", v );

          value = v;

          return DFB_OK;
     }

     return Graphics::Config::GetOption( name, value );
}

DFBResult
X11EGLConfig::CheckOptions( const Graphics::Options &options )
{
     DFBResult ret = DFB_OK;

     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p, options %p )\n", __FUNCTION__, this, &options );

     for (Graphics::Options::const_iterator it = options.begin();
          it != options.end();
          it++)
     {
          long                  val   = 0;
          long                  check = 0;
          Graphics::OptionBase *base = (*it).second;

          D_DEBUG_AT( DFBX11_EGLConfig, "  ---> '%s'\n", *base->GetName() );

          GetOption( base->GetName(), val );

          //check = dynamic_cast<Graphics::Option<long> *>(base)->GetValue();

          EGL::EGLInt egl_int;

          if (FromString<EGL::EGLInt>( egl_int, base->GetString() ))
               check = egl_int.value;
          else
               check = atoi( *base->GetString() );

          if (base->GetName() == "RENDERABLE_TYPE" || base->GetName() == "SURFACE_TYPE") {
               if ((val & check) == check)
                    D_DEBUG_AT( DFBX11_EGLConfig, "  =    local '0x%08lx' contains '0x%08lx'\n", val, check );
               else {
                    D_DEBUG_AT( DFBX11_EGLConfig, "  X    local '0x%08lx' misses '0x%08lx'\n", val, check );

                    ret = DFB_UNSUPPORTED;
               }
          }
          else {
               if (val > check)// && check != 0)
                    D_DEBUG_AT( DFBX11_EGLConfig, "  >    local '%ld' greater than '%ld'\n", val, check );
               else if (val == check)
                    D_DEBUG_AT( DFBX11_EGLConfig, "  =    local '%ld' equals\n", val );
               else {
                    D_DEBUG_AT( DFBX11_EGLConfig, "  X    local '%ld' < '%ld'\n", val, check );

                    ret = DFB_UNSUPPORTED;
               }
          }
     }

     if (ret)
          return ret;

     return Graphics::Config::CheckOptions( options );
}

DFBResult
X11EGLConfig::CreateContext( const Direct::String  &api,
                             Graphics::Context     *share,
                             Graphics::Options     *options,
                             Graphics::Context    **ret_context )
{
     DFBResult          ret;
     Graphics::Context *context;

     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p, api '%s', share %p, options %p )\n",
                 __FUNCTION__, this, *api, share, options );

     context = new X11EGLContext( impl, api, this, share, options );

     ret = context->Init();
     if (ret) {
          delete context;
          return ret;
     }

     *ret_context = context;

     return DFB_OK;
}

DFBResult
X11EGLConfig::CreateSurfacePeer( IDirectFBSurface       *surface,
                                 Graphics::Options      *options,
                                 Graphics::SurfacePeer **ret_peer )
{
     DFBResult              ret;
     Graphics::SurfacePeer *peer;

     D_DEBUG_AT( DFBX11_EGLConfig, "X11EGLConfig::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     peer = new X11EGLSurfacePeer( impl, this, options, surface );

     ret = peer->Init();
     if (ret) {
          delete peer;
          return ret;
     }

     *ret_peer = peer;

     return DFB_OK;
}

/**********************************************************************************************************************/

X11EGLContext::X11EGLContext( X11EGLImpl           &impl,
                              const Direct::String &api,
                              Graphics::Config     *config,
                              Graphics::Context    *share,
                              Graphics::Options    *options )
     :
     Context( api, config, share, options ),
     impl( impl ),
     egl_context( EGL_NO_CONTEXT )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n", __FUNCTION__, this );
}

X11EGLContext::~X11EGLContext()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
X11EGLContext::Init()
{
     X11EGLConfig *config;
     EGLContext    egl_share = EGL_NO_CONTEXT;

     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n", __FUNCTION__, this );

     config = GetConfig<X11EGLConfig>();

     if (share)
          egl_share = dynamic_cast<X11EGLContext*>( share )->egl_context;

     EGLenum egl_api = DirectFB::EGL::Util::StringToAPI( api );

     if (!egl_api) {
          D_ERROR( "X11/EGLImpl: Unknown API '%s'!\n", *api );
          return DFB_INVARG;
     }

     impl.display->Sync();

     impl.lib.eglBindAPI( egl_api );

     EGLint attribs[] = {
          EGL_CONTEXT_CLIENT_VERSION, (EGLint) options->GetValue<long>( "CONTEXT_CLIENT_VERSION", 1 ),
          EGL_NONE
     };

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglCreateContext( EGLDisplay %p, EGLConfig %p, share %p, attribs %p )\n",
                 impl.egl_display, config->egl_config, egl_share, attribs );

     egl_context = impl.lib.eglCreateContext( impl.egl_display, config->egl_config, egl_share, attribs );
     if (egl_context == EGL_NO_CONTEXT) {
          D_ERROR( "X11/EGLImpl: eglCreateContext() failed\n" );
          return DFB_FAILURE;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  => EGLContext %p\n", egl_context );

     impl.display->Sync();

     return DFB_OK;
}

DFBResult
X11EGLContext::Bind( Graphics::SurfacePeer *draw,
                     Graphics::SurfacePeer *read )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n", __FUNCTION__, this );

     X11EGLSurfacePeer *draw_peer = (X11EGLSurfacePeer *) draw;
     X11EGLSurfacePeer *read_peer = (X11EGLSurfacePeer *) read;

     EGLSurface surf_draw = draw_peer->getEGLSurface();
     EGLSurface surf_read = read_peer->getEGLSurface();

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglMakeCurrent( draw/read 0x%08lx/0x%08lx, context 0x%08lx )...\n",
                 (unsigned long) surf_draw, (unsigned long) surf_read, (unsigned long) egl_context );

     impl.display->Sync();

     if (!impl.lib.eglMakeCurrent( impl.egl_display, surf_draw, surf_read, egl_context )) {
          D_ERROR( "X11/EGLImpl: eglMakeCurrent( %p, %p, %p ) failed\n", surf_draw, surf_read, (void*) (long) egl_context );
          return DFB_FAILURE;
     }

     impl.display->Sync();

     return DFB_OK;
}

DFBResult
X11EGLContext::Unbind()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n", __FUNCTION__, this );

     if (!impl.lib.eglMakeCurrent( impl.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )) {
          D_ERROR( "X11/EGLImpl: eglMakeCurrent( EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) failed\n" );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

DFBResult
X11EGLContext::GetProcAddress( const Direct::String  &name,
                               void                 *&addr )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p, name '%s' )\n", __FUNCTION__, this, *name );

     __eglMustCastToProperFunctionPointerType result = impl.lib.eglGetProcAddress( *name );

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> eglGetProcAddress() -> %p\n", result );

     if (result) {
          addr = (void*) result;
          return DFB_OK;
     }

     return DFB_ITEMNOTFOUND;
}

//DFBResult
//X11EGLContext::CreateTask( Graphics::RenderTask *&task )
//{
//     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLContext::%s( %p )\n",  __FUNCTION__, this );
//
//     task = new X11EGLTask();
//
//     return DFB_OK;
//}

/**********************************************************************************************************************/

X11EGLSurfacePeer::X11EGLSurfacePeer( X11EGLImpl        &impl,
                                      Graphics::Config  *config,
                                      Graphics::Options *options,
                                      IDirectFBSurface  *surface )
     :
     SurfacePeer( config, options, surface ),
     impl( impl ),
     alloc_num( 0 ),
     index( CSBR_BACK ),
     is_pixmap( false )
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     memset( alloc_left,  0, sizeof(alloc_left) );
     memset( alloc_right, 0, sizeof(alloc_right) );
}

X11EGLSurfacePeer::~X11EGLSurfacePeer()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     for (unsigned int i=0; i<alloc_num; i++) {
          if (alloc_left[i])
               alloc_left[i]->Release( alloc_left[i] );

          if (alloc_right[i])
               alloc_right[i]->Release( alloc_right[i] );
     }
}

DFBResult
X11EGLSurfacePeer::Init()
{
     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     DFBResult ret;

     ret = SurfacePeer::Init();
     if (ret)
          return ret;

     ret = surface->GetAllocations( surface, "Window/X11", MAX_SURFACE_BUFFERS, &alloc_num, alloc_left, NULL/*alloc_right*/ );
     if (ret) {
          D_LOG( DFBX11_EGLImpl, VERBOSE, "  -> IDirectFBSurface::GetAllocations( 'Window/X11' ) failed! (%s)\n", *ToString<DFBResult>(ret) );

          is_pixmap = true;

          ret = surface->GetAllocations( surface, "Pixmap/X11", MAX_SURFACE_BUFFERS, &alloc_num, alloc_left, NULL/*alloc_right*/ );
          if (ret) {
               D_LOG( DFBX11_EGLImpl, VERBOSE, "  -> IDirectFBSurface::GetAllocations( 'Pixmap/X11' ) failed! (%s)\n", *ToString<DFBResult>(ret) );

               DFBSurfaceCapabilities caps;
               unsigned int           num = 1;

               surface->GetCapabilities( surface, &caps );

               if (caps & DSCAPS_TRIPLE)
                    num = 3;
               else if (caps & DSCAPS_DOUBLE)
                    num = 2;

               for (unsigned int i=0; i<num; i++) {
                    ret = surface->Allocate( surface, (DFBSurfaceBufferRole) i, DSSE_LEFT, "Pixmap/X11", 0, &alloc_left[alloc_num] );
                    if (ret) {
                         D_LOG( DFBX11_EGLImpl, VERBOSE, "  -> IDirectFBSurface::Allocate( %d, LEFT, Pixmap/X11 ) failed! (%s)\n", i, *ToString<DFBResult>(ret) );
                         break;
                    }

                    alloc_num++;
               }
          }
     }

     while (index >= alloc_num)
          index -= alloc_num;

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> %u allocations\n", alloc_num );

     if (alloc_num < 1)
          return DFB_NOALLOCATION;

     return DFB_OK;
}

DFBResult
X11EGLSurfacePeer::Flip( const DFBRegion     *region,
                         DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;

     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     EGLSurface egl_surface = getEGLSurface();

     impl.display->Sync();

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglSwapBuffers( %p )\n", egl_surface );

//     if (is_pixmap) {
//          glFinish();
//     }
//     else {
          if (!impl.lib.eglSwapBuffers( impl.egl_display, egl_surface )) {
               D_ERROR( "X11/EGLImpl: eglSwapBuffers( %p ) failed ('%s')\n", egl_surface, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ));
               return DFB_FAILURE;
          }

          XFlush( impl.x11_display );
//     }
     D_DEBUG_AT( DFBX11_EGLImpl, "  -> eglSwapBuffers( %p ) done\n", egl_surface );

     impl.display->Sync();

     ret = alloc_left[index]->Updated( alloc_left[index], NULL, 0 );
     if (ret) {
          D_DERROR( ret, "X11/EGLImpl: IDirectFBSurfaceAllocation(Window/X11)::Updated() failed!\n" );
          return ret;
     }

     impl.display->Sync();

     ret = SurfacePeer::Flip( region, flags );
     if (ret)
          return ret;

     impl.display->Sync();

     D_ASSERT( alloc_num > 0 );
     if (++index == alloc_num)
          index = 0;

     if (alloc_num > 1) {
          EGLSurface current_draw = impl.lib.eglGetCurrentSurface( EGL_DRAW );
          EGLSurface current_read = impl.lib.eglGetCurrentSurface( EGL_READ );

          D_DEBUG_AT( DFBX11_EGLImpl, "  -> current draw/read 0x%08lx/0x%08lx\n",
                      (unsigned long) current_draw, (unsigned long) current_read );

          if (current_draw == egl_surface || current_read == egl_surface) {
               EGLContext current_context = impl.lib.eglGetCurrentContext();

               D_ASSUME( current_context != EGL_NO_CONTEXT );

               if (current_context != EGL_NO_CONTEXT) {
                    D_DEBUG_AT( DFBX11_EGLImpl, "  -> rebinding context 0x%08lx to new surfaces\n",
                                (unsigned long) current_context );

                    EGLSurface next_draw = getEGLSurface();     // FIMXE: bind context to peer while bound, handle draw/read separately
                    EGLSurface next_read = getEGLSurface();     // FIMXE: bind context to peer while bound, handle draw/read separately

                    D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglMakeCurrent( draw/read 0x%08lx/0x%08lx, context 0x%08lx )...\n",
                                (unsigned long) next_draw, (unsigned long) next_read, (unsigned long) current_context );

                    if (!impl.lib.eglMakeCurrent( impl.egl_display, next_draw, next_read, current_context )) {
                         D_ERROR( "X11/EGLImpl: eglMakeCurrent( %p, %p, %p ) failed\n", next_draw, next_read, (void*) (long) current_context );
                         return DFB_FAILURE;
                    }
               }
          }

     }

     impl.display->Sync();

     return DFB_OK;
}

EGLSurface
X11EGLSurfacePeer::getEGLSurface()
{
     EGLSurface egl_surface = EGL_NO_SURFACE;

     D_DEBUG_AT( DFBX11_EGLImpl, "X11EGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     DFBResult ret;
     u64       handle = 0;

     ret = alloc_left[index]->GetHandle( alloc_left[index], &handle );
     if (ret) {
          D_DERROR( ret, "X11/EGLImpl: IDirectFBSurfaceAllocation(%s/X11)::GetHandle() failed!\n", is_pixmap ? "Pixmap" : "Window" );
          return EGL_NO_SURFACE;
     }

     SurfaceMap::iterator it = surface_map.find( handle );

     if (it == surface_map.end()) {
          X11EGLConfig *config = GetConfig<X11EGLConfig>();
          EGLint surface_attrs[] = {
               EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
               EGL_NONE
          };

          D_DEBUG_AT( DFBX11_EGLImpl, "  -> calling eglCreate%sSurface( EGLConfig %p, %p )\n", is_pixmap ? "Pixmap" : "Window",
                      config->egl_config, (EGLNativePixmapType) handle );

          impl.display->Sync();

          if (is_pixmap)
               egl_surface = impl.lib.eglCreatePixmapSurface( impl.egl_display, config->egl_config,
                                                              (EGLNativePixmapType) handle, NULL );// FIXME: Add Options ToAttribs
          else
               egl_surface = impl.lib.eglCreateWindowSurface( impl.egl_display, config->egl_config,
                                                              (EGLNativeWindowType) handle, surface_attrs );// FIXME: Add Options ToAttribs

          D_DEBUG_AT( DFBX11_EGLImpl, "  -> EGLSurface %p\n", egl_surface );

          if (egl_surface == EGL_NO_SURFACE) {
               D_ERROR( "X11/EGLImpl: eglCreate%sSurface( EGLConfig %p, %s 0x%08lx ) failed (%s)\n",
                        is_pixmap ? "Pixmap" : "Window", config->egl_config, is_pixmap ? "Pixmap" : "Window",
                        (long) handle, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ) );
               return EGL_NO_SURFACE;
          }

          impl.display->Sync();

          surface_map[ handle ] = egl_surface;

          return egl_surface;
     }

     D_DEBUG_AT( DFBX11_EGLImpl, "  -> using cached surface\n" );

     return (*it).second;
}



}

