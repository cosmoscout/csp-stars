////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_STARS_PLUGIN_HPP
#define CSP_STARS_PLUGIN_HPP

#include "../../../src/cs-core/PluginBase.hpp"
#include "../../../src/cs-scene/CelestialAnchorNode.hpp"
#include "../../../src/cs-utils/Property.hpp"
#include "Stars.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <optional>

namespace csp::stars {

/// The starts plugin displays the night sky from star catalogues.
class Plugin : public cs::core::PluginBase {
 public:
  struct Properties {
    cs::utils::Property<bool>   mEnabled                = true;
    cs::utils::Property<bool>   mEnableCelestialGrid    = false;
    cs::utils::Property<bool>   mEnableStarFigures      = false;
    cs::utils::Property<double> mLuminanceMultiplicator = 1.f;
  };

  struct Settings {
    std::string                mBackgroundTexture1;
    std::string                mBackgroundTexture2;
    glm::vec4                  mBackgroundColor1;
    glm::vec4                  mBackgroundColor2;
    std::string                mStarTexture;
    std::optional<std::string> mCacheFile;
    std::optional<std::string> mHipparcosCatalog;
    std::optional<std::string> mTychoCatalog;
    std::optional<std::string> mTycho2Catalog;
  };

  Plugin();

  void init() override;
  void deInit() override;

  void update() override;

 private:
  Settings                                        mPluginSettings;
  std::shared_ptr<Stars>                          mStars;
  std::shared_ptr<cs::scene::CelestialAnchorNode> mStarsTransform;
  VistaOpenGLNode*                                mStarsNode;
  std::shared_ptr<Properties>                     mProperties;

  int mEnableHDRConnection = -1;
};

} // namespace csp::stars

#endif // CSP_STARS_PLUGIN_HPP
