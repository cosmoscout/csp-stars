////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_STARS_VISTA_STARS_HPP
#define CSP_STARS_VISTA_STARS_HPP

#include <VistaBase/VistaColor.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>

#include "../../../src/cs-utils/utils.hpp"

#include <map>
#include <memory>
#include <vector>

class VistaGLSLShader;
class VistaTexture;
class VistaVertexArrayObject;
class VistaBufferObject;

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
  enum class CatalogType { eHipparcos = 0, eTycho, eTycho2, eGaia, eCount };

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

  /// It is possible to load multiple catalogs, currently Hipparcos and any of Tycho or Tycho2 can
  /// be loaded together. Stars which are in both catalogs will be loaded from Hiparcos. Once
  /// loaded, the stars will be written to a binary cache file. Delete this file if you want to load
  /// different catalogs!
  /// @param sStarTextureFile     An uncompressed TGA grayscale image used for the stars.
  /// @param sCacheFile           Location for the star cache.
  Stars(const std::map<CatalogType, std::string>& mCatalogs, const std::string& sStarTextureFile,
      const std::string& sCacheFile = "star_cache.dat");

  /// Creates a new instance of this class.
  /// @param sConfigFile     An XML file containing values for configuring the stars. See demo_01
  ///                        for an example.
  explicit Stars(const std::string& sConfigFile);

  ~Stars() override;

  /// Stars below this magnitude will not be drawn.
  /// Default is -15.f.
  void  setMinMagnitude(float fValue);
  float getMinMagnitude() const;

  /// Stars above this magnitude will not be drawn.
  /// Default is 15.f.
  void  setMaxMagnitude(float fValue);
  float getMaxMagnitude() const;

  /// Sets the size of the faintest stars. Stars with the highest loaded magnitude will be displayed
  /// with this size.
  /// Default is 0.1f.
  /// @param fValue   In percent of the the screen width.
  void  setMinSize(float fValue);
  float getMinSize() const;

  /// Sets the size of the brightest stars. Stars with the lowest loaded magnitude will be displayed
  /// with this size.
  /// Default is 3.f.
  /// @param fValue   In percent of the the screen width.
  void  setMaxSize(float fValue);
  float getMaxSize() const;

  /// Sets the opacity of the faintest stars. Stars with the highest loaded magnitude will be
  /// displayed with this opacity.
  /// Default is 0.7f.
  /// @param fValue   In percent of the the screen width.
  void  setMinOpacity(float fValue);
  float getMinOpacity() const;

  /// Sets the opacity of the brightest stars. Stars with the lowest loaded magnitude will be
  /// displayed with this opacity.
  /// Default is 1.f.
  /// @param fValue   In percent of the the screen width.
  void  setMaxOpacity(float fValue);
  float getMaxOpacity() const;

  /// Configures the mapping from magnitude to appearance parameters. A value of one means linear
  /// mapping from magnitude to size and opacity. Greater values will result in a non-linear mapping
  /// which exaggerates bright stars and results in a more easy to read star field.
  /// Default is 4.0f.
  /// @param fValue    Should be greater or equal to one.
  void  setScalingExponent(float fValue);
  float getScalingExponent() const;

  /// Draws the stars in 3D space.
  /// Default is false.
  /// @param bValue    If set to true, stars are drawn in 3D space.
  void setDraw3D(bool bValue);
  bool getDraw3D() const;

  /// Adds a skydome texture. The given texture is projected via equirectangular projection onto the
  /// background and blended additively.
  /// @param sFilename    A path to an uncompressed TGA image or "" to disable this image.
  void setBackgroundTexture1(const std::string& sFilename);
  void setBackgroundTexture2(const std::string& sFilename);

  /// Colorizes the skydome texture. Since the textures are blended additively, the alpha component
  /// modulates the brightness only.
  /// @param cValue    A RGBA color.
  void              setBackgroundColor1(const VistaColor& cValue);
  const VistaColor& getBackgroundColor1() const;
  void              setBackgroundColor2(const VistaColor& cValue);
  const VistaColor& getBackgroundColor2() const;

  /// Sets the star texture. This texture should be a small (e.g. 64x64) image used for every star.
  /// @param sFilename    A path to an uncompressed grayscale TGA image.
  void setStarTexture(const std::string& sFilename);

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
  void init(const std::string& sStarTextureFile, const std::string& sCacheFile);

  /// Reads star data from binary file.
  bool readStarsFromCatalog(CatalogType eType, const std::string& sFilename);

  /// Writes internal star data read from catalog into a binary file.
  void writeStarCache(const std::string& sCacheFile) const;

  /// Reads star data from binary file.
  bool readStarCache(const std::string& sCacheFile);

  /// Build vertex array objects from given star list.
  void buildStarVAO();
  void buildBackgroundVAO();

  std::shared_ptr<VistaTexture> mStarTexture;
  VistaGLSLShader*              mStarShader;
  VistaGLSLShader*              mBackgroundShader;

  std::shared_ptr<VistaTexture> mBackgroundTexture1;
  std::shared_ptr<VistaTexture> mBackgroundTexture2;
  VistaColor                    mBackgroundColor1;
  VistaColor                    mBackgroundColor2;

  VistaVertexArrayObject* mStarVAO;
  VistaBufferObject*      mStarVBO;

  VistaVertexArrayObject* mBackgroundVAO;
  VistaBufferObject*      mBackgroundVBO;

  std::vector<Star> mStars;

  std::vector<VistaColor> mSpectralColors;

  std::map<CatalogType, std::string> mCatalogs;

  float mLoadedMinMagnitude;
  float mLoadedMaxMagnitude;

  float mMinMagnitude;
  float mMaxMagnitude;
  float mMinSize;
  float mMaxSize;
  float mMinOpacity;
  float mMaxOpacity;
  float mScalingExponent;
  float mDraw3D;

  static const int cCacheVersion;
  static const int cColumnMapping[cs::utils::enumCast(CatalogType::eCount)]
                                 [cs::utils::enumCast(CatalogColumn::eCount)];

  static const std::string cStarsVert;
  static const std::string cStarsFrag;
  static const std::string cStarsGeom;
  static const std::string cBackgroundVert;
  static const std::string cBackgroundFrag;
};

} // namespace csp::stars

#endif // CSP_STARS_VISTA_STARS_HPP
