////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_STARS_VISTA_STARS_HPP
#define CSP_STARS_VISTA_STARS_HPP

#include <VistaBase/VistaColor.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>
#include <VistaOGLExt/VistaBufferObject.h>
#include <VistaOGLExt/VistaGLSLShader.h>
#include <VistaOGLExt/VistaVertexArrayObject.h>

#include "../../../src/cs-utils/utils.hpp"

#include <map>
#include <memory>
#include <vector>

class VistaTexture;

namespace csp::stars {

/// If added to the scene graph, this will draw a configurable star background. It is possible to
/// limit the drawn stars by magnitude, adjust their size, texture and opacity. Furthermore it is
/// possible to draw multiple sky dome images additively on top in order to visualize additional
/// information such as a constellations or grid lines.
class Stars : public IVistaOpenGLDraw {
 public:
  /// The supported catalog Types.
  /// Hipparcos and Tycho can be obtained from:
  ///    http://cdsarc.u-strasbg.fr/viz-bin/Cat?cat=I%2F239
  /// Tycho2 can be obtained from:
  ///    http://cdsarc.u-strasbg.fr/cgi-bin/myqcat3?I/259/
  enum class CatalogType { eHipparcos = 0, eTycho, eTycho2, eCount };

  /// The required columns of each catalog. The position of each column in each catalog is
  /// configured with the static member COLUMN_MAPPING at the bottom of this file.
  enum class CatalogColumn {
    eVmag = 0, ///< visual magnitude
    eBmag,     ///< blue magnitude
    ePara,     ///< trigonometric parallax
    eRect,     ///< rectascension
    eDecl,     ///< declination
    eHipp,     ///< hipparcos number
    eCount
  };

  enum DrawMode { ePoint, eSmoothPoint, eDisc, eSmoothDisc, eSprite };

  /// It is possible to load multiple catalogs, currently Hipparcos and any of Tycho or Tycho2 can
  /// be loaded together. Stars which are in both catalogs will be loaded from Hiparcos. Once
  /// loaded, the stars will be written to a binary cache file. Delete this file if you want to load
  /// different catalogs!
  /// @param sStarTextureFile     An uncompressed TGA grayscale image used for the stars.
  /// @param sCacheFile           Location for the star cache.
  Stars(const std::map<CatalogType, std::string>& catalogs, const std::string& starTexture,
      const std::string& cacheFile = "star_cache.dat");

  /// Specifies how the stars should be drawn.
  void     setDrawMode(DrawMode value);
  DrawMode getDrawMode() const;

  /// Sets the size of the stars.
  /// Stars will be drawn covering this solid angle. This has no effect if DrawMode
  /// is set to ePoint. Default is 0.01f.
  /// @param value   In steradians.
  void  setSolidAngle(float value);
  float getSolidAngle() const;

  /// When set to true, stars will be drawn with true luminance values. Else their brightness will
  /// be between 0 and 1.
  void setEnableHDR(bool value);
  bool getEnableHDR() const;

  /// Stars below this magnitude will not be drawn.
  /// Default is -15.f.
  void  setMinMagnitude(float value);
  float getMinMagnitude() const;

  /// Stars above this magnitude will not be drawn.
  /// Default is 15.f.
  void  setMaxMagnitude(float value);
  float getMaxMagnitude() const;

  void  setLuminanceMultiplicator(float value);
  float getLuminanceMultiplicator() const;

  /// Adds a skydome texture. The given texture is projected via equirectangular projection onto the
  /// background and blended additively.
  /// @param sFilename    A path to an uncompressed TGA image or "" to disable this image.
  void setBackgroundTexture1(const std::string& filename);
  void setBackgroundTexture2(const std::string& filename);

  /// Colorizes the skydome texture. Since the textures are blended additively, the alpha component
  /// modulates the brightness only.
  /// @param cValue    A RGBA color.
  void              setBackgroundColor1(const VistaColor& value);
  const VistaColor& getBackgroundColor1() const;
  void              setBackgroundColor2(const VistaColor& value);
  const VistaColor& getBackgroundColor2() const;

  /// Sets the star texture. This texture should be a small (e.g. 64x64) image used for every star.
  /// @param sFilename    A path to an uncompressed grayscale TGA image.
  void setStarTexture(const std::string& filename);

  /// The method Do() gets the callback from scene graph during the rendering process.
  bool Do() override;

  /// This method should return the bounding box of the openGL object you draw in the method Do().
  bool GetBoundingBox(VistaBoundingBox& bb) override;

 private:
  /// Data structure of one record from star catalog.
  struct Star {
    float mVMagnitude;
    float mBMagnitude;
    float mAscension;
    float mDeclination;
    float mParallax;
  };

  /// Called by the constructors.
  void init(const std::string& textureFile, const std::string& cacheFile);

  /// Reads star data from binary file.
  bool readStarsFromCatalog(CatalogType type, const std::string& filename);

  /// Writes internal star data read from catalog into a binary file.
  void writeStarCache(const std::string& cacheFile) const;

  /// Reads star data from binary file.
  bool readStarCache(const std::string& cacheFile);

  /// Build vertex array objects from given star list.
  void buildStarVAO();
  void buildBackgroundVAO();

  std::shared_ptr<VistaTexture>    mStarTexture;
  std::unique_ptr<VistaGLSLShader> mStarShader;
  std::unique_ptr<VistaGLSLShader> mBackgroundShader;
  std::shared_ptr<VistaTexture>    mBackgroundTexture1;
  std::shared_ptr<VistaTexture>    mBackgroundTexture2;
  VistaColor                       mBackgroundColor1;
  VistaColor                       mBackgroundColor2;

  std::unique_ptr<VistaVertexArrayObject> mStarVAO       = nullptr;
  std::unique_ptr<VistaBufferObject>      mStarVBO       = nullptr;
  std::unique_ptr<VistaVertexArrayObject> mBackgroundVAO = nullptr;
  std::unique_ptr<VistaBufferObject>      mBackgroundVBO = nullptr;

  std::vector<Star>                  mStars;
  std::vector<VistaColor>            mSpectralColors;
  std::map<CatalogType, std::string> mCatalogs;

  DrawMode mDrawMode = DrawMode::eSmoothPoint;

  bool  mShaderDirty            = true;
  bool  mEnableHDR              = true;
  float mSolidAngle             = 0.00001f;
  float mMinMagnitude           = -5.f;
  float mMaxMagnitude           = 15.f;
  float mLuminanceMultiplicator = 1.f;

  static const int cCacheVersion;
  static const int cColumnMapping[cs::utils::enumCast(CatalogType::eCount)]
                                 [cs::utils::enumCast(CatalogColumn::eCount)];

  static const std::string cStarsSnippets;
  static const std::string cStarsVertOnePixel;
  static const std::string cStarsFragOnePixel;
  static const std::string cStarsVert;
  static const std::string cStarsFrag;
  static const std::string cStarsGeom;
  static const std::string cBackgroundVert;
  static const std::string cBackgroundFrag;
};

} // namespace csp::stars

#endif // CSP_STARS_VISTA_STARS_HPP
