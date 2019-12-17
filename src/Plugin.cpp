////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"

#include "../../../src/cs-core/GraphicsEngine.hpp"
#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"

#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::stars::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::stars {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings& o) {
  cs::core::parseSection("csp-stars", [&] {
    o.mBackgroundTexture1 = cs::core::parseProperty<std::string>("backgroundTexture1", j);
    o.mBackgroundTexture2 = cs::core::parseProperty<std::string>("backgroundTexture2", j);

    auto b1 = j.at("backgroundColor1");
    for (int i = 0; i < 4; ++i) {
      o.mBackgroundColor1[i] = b1.at(i);
    }

    auto b2 = j.at("backgroundColor2");
    for (int i = 0; i < 4; ++i) {
      o.mBackgroundColor2[i] = b2.at(i);
    }

    o.mStarTexture = cs::core::parseProperty<std::string>("starTexture", j);

    o.mCacheFile        = cs::core::parseOptional<std::string>("cacheFile", j);
    o.mHipparcosCatalog = cs::core::parseOptional<std::string>("hipparcosCatalog", j);
    o.mTychoCatalog     = cs::core::parseOptional<std::string>("tychoCatalog", j);
    o.mTycho2Catalog    = cs::core::parseOptional<std::string>("tycho2Catalog", j);
  });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Plugin::Plugin()
    : mProperties(std::make_shared<Properties>()) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {
  std::cout << "Loading: CosmoScout VR Plugin Stars" << std::endl;

  // init stars
  mPluginSettings = mAllSettings->mPlugins.at("csp-stars");

  std::map<Stars::CatalogType, std::string> catalogs;

  if (mPluginSettings.mHipparcosCatalog) {
    catalogs[Stars::CatalogType::eHipparcos] = *mPluginSettings.mHipparcosCatalog;
  }

  if (mPluginSettings.mTychoCatalog) {
    catalogs[Stars::CatalogType::eTycho] = *mPluginSettings.mTychoCatalog;
  }

  if (mPluginSettings.mTycho2Catalog) {
    catalogs[Stars::CatalogType::eTycho2] = *mPluginSettings.mTycho2Catalog;
  }

  std::string cacheFile = "star_cache.dat";
  if (mPluginSettings.mCacheFile) {
    cacheFile = *mPluginSettings.mCacheFile;
  }

  mStars = std::make_shared<Stars>(catalogs, mPluginSettings.mStarTexture, cacheFile);

  // set star settings
  auto& bg1 = mPluginSettings.mBackgroundColor1;
  auto& bg2 = mPluginSettings.mBackgroundColor2;

  mStars->setBackgroundTexture1(mPluginSettings.mBackgroundTexture1);
  mStars->setBackgroundTexture2(mPluginSettings.mBackgroundTexture2);

  mStars->setBackgroundColor1(VistaColor(bg1.r, bg1.g, bg1.b, bg1.a));
  mStars->setBackgroundColor2(VistaColor(bg2.r, bg2.g, bg2.b, bg2.a));

  // add to scenegraph
  mStarsTransform = std::make_shared<cs::scene::CelestialAnchorNode>(
      mSceneGraph->GetRoot(), mSceneGraph->GetNodeBridge(), "", "Solar System Barycenter", "J2000");
  mSolarSystem->registerAnchor(mStarsTransform);

  mSceneGraph->GetRoot()->AddChild(mStarsTransform.get());

  mStarsNode = mSceneGraph->NewOpenGLNode(mStarsTransform.get(), mStars.get());

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mStarsTransform.get(), static_cast<int>(cs::utils::DrawOrder::eStars));

  mProperties->mEnabled.onChange().connect(
      [this](bool val) { this->mStarsNode->SetIsEnabled(val); });

  mGuiManager->addSettingsSectionToSideBarFromHTML(
      "Stars", "star", "../share/resources/gui/stars_settings.html");

  mGuiManager->addScriptToSideBarFromJS("../share/resources/gui/js/stars_settings.js");

  mGuiManager->getSideBar()->registerCallback<bool>(
      "set_enable_stars", ([this](bool value) { mProperties->mEnabled = value; }));

  mGuiManager->getSideBar()->registerCallback<bool>(
      "set_enable_stars_grid", ([this](bool value) { mProperties->mEnableCelestialGrid = value; }));

  mGuiManager->getSideBar()->registerCallback<bool>("set_enable_stars_figures",
      ([this](bool value) { mProperties->mEnableStarFigures = value; }));

  mGuiManager->getSideBar()->registerCallback<double>("set_star_luminance_boost",
      ([this](double value) { mProperties->mLuminanceMultiplicator = std::exp(value); }));

  mGuiManager->getSideBar()->registerCallback<double>(
      "set_star_size", ([this](double value) { mStars->setSolidAngle(value * 0.0001); }));

  mGuiManager->getSideBar()->registerCallback<double, double>(
      "set_star_magnitude", ([this](double val, double handle) {
        if (handle == 0.0)
          mStars->setMinMagnitude(val);
        else
          mStars->setMaxMagnitude(val);
      }));

  mGuiManager->getSideBar()->registerCallback(
      "set_star_draw_mode_0", ([this]() { mStars->setDrawMode(Stars::ePoint); }));

  mGuiManager->getSideBar()->registerCallback(
      "set_star_draw_mode_1", ([this]() { mStars->setDrawMode(Stars::eSmoothPoint); }));

  mGuiManager->getSideBar()->registerCallback(
      "set_star_draw_mode_2", ([this]() { mStars->setDrawMode(Stars::eDisc); }));

  mGuiManager->getSideBar()->registerCallback(
      "set_star_draw_mode_3", ([this]() { mStars->setDrawMode(Stars::eSmoothDisc); }));

  mGuiManager->getSideBar()->registerCallback(
      "set_star_draw_mode_4", ([this]() { mStars->setDrawMode(Stars::eSprite); }));

  mGraphicsEngine->pEnableHDR.onChange().connect(
      [this](bool value) { mStars->setEnableHDR(value); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  mSolarSystem->unregisterAnchor(mStarsTransform);
  mSceneGraph->GetRoot()->DisconnectChild(mStarsTransform.get());

  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars");
  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars_grid");
  mGuiManager->getSideBar()->unregisterCallback("set_enable_stars_figures");
  mGuiManager->getSideBar()->unregisterCallback("set_star_luminance_boost");
  mGuiManager->getSideBar()->unregisterCallback("set_star_size");
  mGuiManager->getSideBar()->unregisterCallback("set_star_magnitude");
  mGuiManager->getSideBar()->unregisterCallback("set_star_draw_mode_0");
  mGuiManager->getSideBar()->unregisterCallback("set_star_draw_mode_1");
  mGuiManager->getSideBar()->unregisterCallback("set_star_draw_mode_2");
  mGuiManager->getSideBar()->unregisterCallback("set_star_draw_mode_3");
  mGuiManager->getSideBar()->unregisterCallback("set_star_draw_mode_4");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  float fIntensity = mGraphicsEngine->pApproximateSceneBrightness.get();

  if (mGraphicsEngine->pEnableHDR.get()) {
    fIntensity = 1.f;
  }

  mStars->setLuminanceMultiplicator(fIntensity * mProperties->mLuminanceMultiplicator.get());
  mStars->setBackgroundColor1(VistaColor(
      0.5f, 0.8f, 1.f, 0.3f * fIntensity * (mProperties->mEnableCelestialGrid.get() ? 1.f : 0.f)));
  mStars->setBackgroundColor2(VistaColor(
      0.5f, 1.f, 0.8f, 0.3f * fIntensity * (mProperties->mEnableStarFigures.get() ? 1.f : 0.f)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
