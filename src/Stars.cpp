////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Stars.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include "../../../src/cs-graphics/TextureLoader.hpp"

#include <VistaInterProcComm/Connections/VistaByteBufferDeSerializer.h>
#include <VistaInterProcComm/Connections/VistaByteBufferSerializer.h>
#include <VistaKernel/GraphicsManager/VistaGeometryFactory.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaOGLExt/VistaBufferObject.h>
#include <VistaOGLExt/VistaGLSLShader.h>
#include <VistaOGLExt/VistaOGLUtils.h>
#include <VistaOGLExt/VistaTexture.h>
#include <VistaOGLExt/VistaVertexArrayObject.h>
#include <VistaTools/tinyXML/tinyxml.h>
#include <spdlog/spdlog.h>

#include <fstream>

namespace csp::stars {

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool FromString(std::string const& v, T& out) {
  std::istringstream iss(v);
  iss >> out;
  return (iss.rdstate() & std::stringstream::failbit) == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

const int Stars::cColumnMapping[cs::utils::enumCast(CatalogType::eCount)]
                               [cs::utils::enumCast(CatalogColumn::eCount)] = {
                                   {34, 32, 11, 8, 9, 31}, // CatalogType::eHipparcos
                                   {34, 32, 11, 8, 9, 31}, // CatalogType::eTycho
                                   {19, 17, -1, 2, 3, 23}, // CatalogType::eTycho2
                                   {50, 55, 9, 5, 7, -1}   // CatalogType::eGaia
};

////////////////////////////////////////////////////////////////////////////////////////////////////

// Increase this if the cache format changed and is incompatible now. This will
// force a reload.
const int Stars::cCacheVersion = 3;

////////////////////////////////////////////////////////////////////////////////////////////////////

Stars::Stars(const std::map<CatalogType, std::string>& mCatalogs,
    const std::string& sStarTextureFile, const std::string& sCacheFile)
    : mBackgroundColor1()
    , mBackgroundColor2()
    , mCatalogs(mCatalogs)
    , mLoadedMinMagnitude(std::numeric_limits<float>::max())
    , mLoadedMaxMagnitude(std::numeric_limits<float>::min())
    , mMinMagnitude(-15.f)
    , mMaxMagnitude(10.f)
    , mMinSize(0.1f)
    , mMaxSize(3.f)
    , mMinOpacity(0.7f)
    , mMaxOpacity(1.f)
    , mScalingExponent(4.f)
    , mDraw3D(false) {
  init(sStarTextureFile, sCacheFile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Stars::Stars(const std::string& sConfigFile)
    : mBackgroundColor1()
    , mBackgroundColor2()
    , mCatalogs()
    , mLoadedMinMagnitude(std::numeric_limits<float>::max())
    , mLoadedMaxMagnitude(std::numeric_limits<float>::min())
    , mMinMagnitude(-15.f)
    , mMaxMagnitude(10.f)
    , mMinSize(0.1f)
    , mMaxSize(3.f)
    , mMinOpacity(0.7f)
    , mMaxOpacity(1.f)
    , mScalingExponent(4.f)
    , mDraw3D(false) {

  VistaXML::TiXmlDocument xDoc(sConfigFile);

  if (!xDoc.LoadFile()) {
    spdlog::error("Failed to load star config file {}: Cannont open file!", sConfigFile);
    return;
  }

  // Get First Element
  const VistaXML::TiXmlElement* pRoot(xDoc.FirstChildElement());

  // Read Data
  if (std::string(pRoot->Value()) != "StarConfig") {
    spdlog::error("Failed to read star config file {}!", sConfigFile);
    return;
  }

  const VistaXML::TiXmlElement* pProperty(pRoot->FirstChildElement());

  std::string sBackgroundTexture1, sBackgroundTexture2, sStarTexture, sSpectralColors,
      sCacheFile("star_cache.dat");

  while (pProperty != nullptr) {
    if (std::string(pProperty->Value()) == "Property") {
      std::string       sName(pProperty->Attribute("Name"));
      std::stringstream ssValue(pProperty->Attribute("Value"));
      if (sName == "MinMagnitude")
        ssValue >> mMinMagnitude;
      else if (sName == "MaxMagnitude")
        ssValue >> mMaxMagnitude;
      else if (sName == "MinSize")
        ssValue >> mMinSize;
      else if (sName == "MaxSize")
        ssValue >> mMaxSize;
      else if (sName == "MinOpacity")
        ssValue >> mMinOpacity;
      else if (sName == "MaxOpacity")
        ssValue >> mMaxOpacity;
      else if (sName == "ScalingExponent")
        ssValue >> mScalingExponent;
      else if (sName == "Draw3D")
        ssValue >> mDraw3D;
      else if (sName == "BackgroundTexture1")
        ssValue >> sBackgroundTexture1;
      else if (sName == "BackgroundTexture2")
        ssValue >> sBackgroundTexture2;
      else if (sName == "StarTexture")
        ssValue >> sStarTexture;
      else if (sName == "HipparcosCatalog")
        ssValue >> mCatalogs[CatalogType::eHipparcos];
      else if (sName == "TychoCatalog")
        ssValue >> mCatalogs[CatalogType::eTycho];
      else if (sName == "Tycho2Catalog")
        ssValue >> mCatalogs[CatalogType::eTycho2];
      else if (sName == "BackgroundColor1") {
        ssValue >> mBackgroundColor1[0] >> mBackgroundColor1[1] >> mBackgroundColor1[2] >>
            mBackgroundColor1[3];
      } else if (sName == "BackgroundColor2") {
        ssValue >> mBackgroundColor2[0] >> mBackgroundColor2[1] >> mBackgroundColor2[2] >>
            mBackgroundColor2[3];
      } else {
        spdlog::warn(
            "Ignoring invalid entity {} while reading star config file {}", sName, sConfigFile);
      }
    } else {
      spdlog::warn("Ignoring invalid entity {} while reading star config file {}",
          pProperty->Value(), sConfigFile);
    }

    pProperty = pProperty->NextSiblingElement();
  }

  if (!sBackgroundTexture1.empty())
    setBackgroundTexture1(sBackgroundTexture1);
  if (!sBackgroundTexture2.empty())
    setBackgroundTexture2(sBackgroundTexture2);

  if (sStarTexture.empty()) {
    spdlog::error("Failed to load star config file {}: StarTexture has to be set!", sConfigFile);
    return;
  }

  init(sStarTexture, sCacheFile);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Stars::~Stars() {
  delete mStarShader;
  delete mBackgroundShader;
  delete mStarVAO;
  delete mStarVBO;
  delete mBackgroundVAO;
  delete mBackgroundVBO;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMinMagnitude(float fValue) {
  mMinMagnitude = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMinMagnitude() const {
  return mMinMagnitude;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMaxMagnitude(float fValue) {
  mMaxMagnitude = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMaxMagnitude() const {
  return mMaxMagnitude;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMinSize(float fValue) {
  mMinSize = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMinSize() const {
  return mMinSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMaxSize(float fValue) {
  mMaxSize = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMaxSize() const {
  return mMaxSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMinOpacity(float fValue) {
  mMinOpacity = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMinOpacity() const {
  return mMinOpacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setMaxOpacity(float fValue) {
  mMaxOpacity = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getMaxOpacity() const {
  return mMaxOpacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setScalingExponent(float fValue) {
  mScalingExponent = fValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

float Stars::getScalingExponent() const {
  return mScalingExponent;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setDraw3D(bool bValue) {
  mDraw3D = bValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::getDraw3D() const {
  return (bool)mDraw3D;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundColor1(const VistaColor& cValue) {
  mBackgroundColor1 = cValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const VistaColor& Stars::getBackgroundColor1() const {
  return mBackgroundColor1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundColor2(const VistaColor& cValue) {
  mBackgroundColor2 = cValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const VistaColor& Stars::getBackgroundColor2() const {
  return mBackgroundColor2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setStarTexture(const std::string& sFilename) {
  if (!sFilename.empty()) {
    mStarTexture = cs::graphics::TextureLoader::loadFromFile(sFilename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundTexture1(const std::string& sFilename) {
  if (!sFilename.empty()) {
    mBackgroundTexture1 = cs::graphics::TextureLoader::loadFromFile(sFilename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::setBackgroundTexture2(const std::string& sFilename) {
  if (!sFilename.empty()) {
    mBackgroundTexture2 = cs::graphics::TextureLoader::loadFromFile(sFilename);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::Do() {
  // save current state of the OpenGL state machine
  glPushAttrib(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  glDepthMask(GL_FALSE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  // get matrices ------------------------------------------------------------
  GLfloat glMat[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, &glMat[0]);
  VistaTransformMatrix matModelView(glMat, true);

  glGetFloatv(GL_PROJECTION_MATRIX, &glMat[0]);
  VistaTransformMatrix matProjection(glMat, true);

  // draw background ---------------------------------------------------------
  if ((mBackgroundTexture1 && mBackgroundColor1[3] != 0.f) ||
      (mBackgroundTexture2 && mBackgroundColor2[3] != 0.f)) {
    mBackgroundVAO->Bind();
    mBackgroundShader->Bind();
    mBackgroundShader->SetUniform(mBackgroundShader->GetUniformLocation("iTexture"), 0);

    VistaTransformMatrix matMVNoTranslation = matModelView;

    // reduce jitter
    matMVNoTranslation[0][3] = 0.f;
    matMVNoTranslation[1][3] = 0.f;
    matMVNoTranslation[2][3] = 0.f;

    VistaTransformMatrix matMVP(matProjection * matMVNoTranslation);
    VistaTransformMatrix matInverseMVP(matMVP.GetInverted());
    VistaTransformMatrix matInverseMV(matMVNoTranslation.GetInverted());

    GLint loc = mBackgroundShader->GetUniformLocation("uInvMVP");
    glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMVP.GetData());

    loc = mBackgroundShader->GetUniformLocation("uInvMV");
    glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMV.GetData());

    if (mBackgroundTexture1 && mBackgroundColor1[3] != 0.f) {
      mBackgroundShader->SetUniform(mBackgroundShader->GetUniformLocation("cColor"),
          mBackgroundColor1[0], mBackgroundColor1[1], mBackgroundColor1[2], mBackgroundColor1[3]);
      mBackgroundTexture1->Bind(GL_TEXTURE0);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      mBackgroundTexture1->Unbind(GL_TEXTURE0);
    }

    if (mBackgroundTexture2 && mBackgroundColor2[3] != 0.f) {
      mBackgroundShader->SetUniform(mBackgroundShader->GetUniformLocation("cColor"),
          mBackgroundColor2[0], mBackgroundColor2[1], mBackgroundColor2[2], mBackgroundColor2[3]);
      mBackgroundTexture2->Bind(GL_TEXTURE0);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      mBackgroundTexture2->Unbind(GL_TEXTURE0);
    }

    mBackgroundShader->Release();
    mBackgroundVAO->Release();
  }

  // draw stars --------------------------------------------------------------
  mStarVAO->Bind();
  mStarShader->Bind();

  mStarTexture->Bind(GL_TEXTURE0);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("iStarTexture"), 0);
  mStarShader->SetUniform(
      mStarShader->GetUniformLocation("fLoadedMinMagnitude"), mLoadedMinMagnitude);
  mStarShader->SetUniform(
      mStarShader->GetUniformLocation("fLoadedMaxMagnitude"), mLoadedMaxMagnitude);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMinMagnitude"), mMinMagnitude);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMaxMagnitude"), mMaxMagnitude);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMinOpacity"), mMinOpacity);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMaxOpacity"), mMaxOpacity);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMinSize"), mMinSize);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fMaxSize"), mMaxSize);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("fScalingExponent"), mScalingExponent);
  mStarShader->SetUniform(mStarShader->GetUniformLocation("bDraw3D"), mDraw3D);

  VistaTransformMatrix matInverseMV(matModelView.GetInverted());

  GLint loc = mStarShader->GetUniformLocation("uMatMV");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matModelView.GetData());

  loc = mStarShader->GetUniformLocation("uMatP");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matProjection.GetData());

  loc = mStarShader->GetUniformLocation("uInvMV");
  glUniformMatrix4fv(loc, 1, GL_FALSE, matInverseMV.GetData());

  glDrawArrays(GL_POINTS, 0, (GLsizei)mStars.size());

  mStarTexture->Unbind(GL_TEXTURE0);

  mStarShader->Release();
  mStarVAO->Release();

  glDepthMask(GL_TRUE);
  glPopAttrib();

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::GetBoundingBox(VistaBoundingBox& oBoundingBox) {
  float min(std::numeric_limits<float>::min());
  float max(std::numeric_limits<float>::max());
  float fMin[3] = {min, min, min};
  float fMax[3] = {max, max, max};

  oBoundingBox.SetBounds(fMin, fMax);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::init(const std::string& sStarTextureFile, const std::string& sCacheFile) {
  // spectral colors from B-V index -0.4 to 2.0 in steps of 0.05
  // values from  http://www.vendian.org/mncharity/dir3/starcolor/details.html
  mSpectralColors.emplace_back(VistaColor(0x9bb2ff));
  mSpectralColors.emplace_back(VistaColor(0x9eb5ff));
  mSpectralColors.emplace_back(VistaColor(0xa3b9ff));
  mSpectralColors.emplace_back(VistaColor(0xaabfff));
  mSpectralColors.emplace_back(VistaColor(0xb2c5ff));
  mSpectralColors.emplace_back(VistaColor(0xbbccff));
  mSpectralColors.emplace_back(VistaColor(0xc4d2ff));
  mSpectralColors.emplace_back(VistaColor(0xccd8ff));
  mSpectralColors.emplace_back(VistaColor(0xd3ddff));
  mSpectralColors.emplace_back(VistaColor(0xdae2ff));
  mSpectralColors.emplace_back(VistaColor(0xdfe5ff));
  mSpectralColors.emplace_back(VistaColor(0xe4e9ff));
  mSpectralColors.emplace_back(VistaColor(0xe9ecff));
  mSpectralColors.emplace_back(VistaColor(0xeeefff));
  mSpectralColors.emplace_back(VistaColor(0xf3f2ff));
  mSpectralColors.emplace_back(VistaColor(0xf8f6ff));
  mSpectralColors.emplace_back(VistaColor(0xfef9ff));
  mSpectralColors.emplace_back(VistaColor(0xfff9fb));
  mSpectralColors.emplace_back(VistaColor(0xfff7f5));
  mSpectralColors.emplace_back(VistaColor(0xfff5ef));
  mSpectralColors.emplace_back(VistaColor(0xfff3ea));
  mSpectralColors.emplace_back(VistaColor(0xfff1e5));
  mSpectralColors.emplace_back(VistaColor(0xffefe0));
  mSpectralColors.emplace_back(VistaColor(0xffeddb));
  mSpectralColors.emplace_back(VistaColor(0xffebd6));
  mSpectralColors.emplace_back(VistaColor(0xffe8ce));
  mSpectralColors.emplace_back(VistaColor(0xffe6ca));
  mSpectralColors.emplace_back(VistaColor(0xffe5c6));
  mSpectralColors.emplace_back(VistaColor(0xffe3c3));
  mSpectralColors.emplace_back(VistaColor(0xffe2bf));
  mSpectralColors.emplace_back(VistaColor(0xffe0bb));
  mSpectralColors.emplace_back(VistaColor(0xffdfb8));
  mSpectralColors.emplace_back(VistaColor(0xffddb4));
  mSpectralColors.emplace_back(VistaColor(0xffdbb0));
  mSpectralColors.emplace_back(VistaColor(0xffdaad));
  mSpectralColors.emplace_back(VistaColor(0xffd8a9));
  mSpectralColors.emplace_back(VistaColor(0xffd6a5));
  mSpectralColors.emplace_back(VistaColor(0xffd29c));
  mSpectralColors.emplace_back(VistaColor(0xffd096));
  mSpectralColors.emplace_back(VistaColor(0xffcc8f));
  mSpectralColors.emplace_back(VistaColor(0xffc885));
  mSpectralColors.emplace_back(VistaColor(0xffc178));
  mSpectralColors.emplace_back(VistaColor(0xffb765));
  mSpectralColors.emplace_back(VistaColor(0xffa94b));
  mSpectralColors.emplace_back(VistaColor(0xff9523));
  mSpectralColors.emplace_back(VistaColor(0xff7b00));
  mSpectralColors.emplace_back(VistaColor(0xff5200));

  // read star catalog -------------------------------------------------------
  if (!readStarCache(sCacheFile)) {
    std::map<CatalogType, std::string>::const_iterator it;

    it = mCatalogs.find(CatalogType::eGaia);
    if (it != mCatalogs.end()) {
      readStarsFromCatalog(it->first, it->second);

      if (mCatalogs.size() > 1) {
        spdlog::error(
            "Failed to load star catalogs: Gaia cannot be combined with other catalogues!");
      }
    } else {
      it = mCatalogs.find(CatalogType::eHipparcos);
      if (it != mCatalogs.end())
        readStarsFromCatalog(it->first, it->second);

      it = mCatalogs.find(CatalogType::eTycho);
      if (it != mCatalogs.end())
        readStarsFromCatalog(it->first, it->second);

      it = mCatalogs.find(CatalogType::eTycho2);
      if (it != mCatalogs.end()) {
        // do not load tycho and tycho 2
        if (mCatalogs.find(CatalogType::eTycho) == mCatalogs.end()) {
          readStarsFromCatalog(it->first, it->second);
        } else {
          spdlog::error("Failed to load Tycho2 catalog: Tycho already loaded!");
        }
      }
    }

    if (!mStars.empty()) {
      writeStarCache(sCacheFile);
    } else {
      spdlog::warn("Loaded no stars. Stars will not work properly.");
    }
  }

  // create texture ----------------------------------------------------------
  mStarTexture = cs::graphics::TextureLoader::loadFromFile(sStarTextureFile);

  // create shaders ----------------------------------------------------------
  mStarShader = new VistaGLSLShader();
  mStarShader->InitVertexShaderFromString(cStarsVert);
  mStarShader->InitFragmentShaderFromString(cStarsFrag);
  mStarShader->InitGeometryShaderFromString(cStarsGeom);
  mStarShader->Link();

  mBackgroundShader = new VistaGLSLShader();
  mBackgroundShader->InitVertexShaderFromString(cBackgroundVert);
  mBackgroundShader->InitFragmentShaderFromString(cBackgroundFrag);
  mBackgroundShader->Link();

  // create buffers ----------------------------------------------------------
  mStarVBO = new VistaBufferObject();
  mStarVAO = new VistaVertexArrayObject();

  mBackgroundVBO = new VistaBufferObject();
  mBackgroundVAO = new VistaVertexArrayObject();

  buildStarVAO();
  buildBackgroundVAO();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::readStarsFromCatalog(CatalogType eType, const std::string& sFilename) {
  bool success = false;
  spdlog::info("Reading {}...", sFilename);

  std::ifstream file;

  try {
    file.open(sFilename.c_str(), std::ifstream::in);
  } catch (std::exception& e) {
    spdlog::error("Failed to open catalog file {}: {}", sFilename, e.what());
  }

  if (file.is_open()) {
    int  lineCount = 0;
    bool loadHipparcos(mCatalogs.find(CatalogType::eHipparcos) != mCatalogs.end());

    // read line by line
    while (!file.eof()) {
      // get line
      ++lineCount;
      std::string line;
      getline(file, line);

      // parse line:
      // separate complete items consisting of "val0|val1|...|valN|" into vector of value
      // strings
      std::stringstream        stream(line);
      std::string              item;
      std::vector<std::string> items;
      char                     delim = (eType == CatalogType::eGaia) ? ',' : '|';

      while (getline(stream, item, delim)) {
        items.emplace_back(item);
      }

      // convert value strings to int/double/float and save in star data structure
      // expecting Hipparcos or Tycho-1 catalog and more than 12 columns
      if (items.size() > 12) {
        // skip if part of hipparcos catalogue
        int tmp;
        if (eType != CatalogType::eHipparcos && loadHipparcos &&
            FromString<int>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                [cs::utils::enumCast(CatalogColumn::eHipp)]],
                tmp)) {
          continue;
        }

        // store star data
        bool successStoreData(true);

        Star star;
        successStoreData &=
            FromString<float>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                  [cs::utils::enumCast(CatalogColumn::eVmag)]],
                star.mVMagnitude);
        successStoreData &=
            FromString<float>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                  [cs::utils::enumCast(CatalogColumn::eBmag)]],
                star.mBMagnitude);
        successStoreData &=
            FromString<float>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                  [cs::utils::enumCast(CatalogColumn::eRect)]],
                star.mAscension);
        successStoreData &=
            FromString<float>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                  [cs::utils::enumCast(CatalogColumn::eDecl)]],
                star.mDeclination);

        if (cColumnMapping[cs::utils::enumCast(eType)][cs::utils::enumCast(CatalogColumn::ePara)] >
            0) {
          if (!FromString<float>(items[cColumnMapping[cs::utils::enumCast(eType)]
                                                     [cs::utils::enumCast(CatalogColumn::ePara)]],
                  star.mParallax)) {
            star.mParallax = 0;
          }
        } else {
          star.mParallax = 0;
        }

        if (successStoreData) {
          star.mAscension   = (360.f + 90.f - star.mAscension) / 180.f * Vista::Pi;
          star.mDeclination = star.mDeclination / 180.f * Vista::Pi;

          mStars.emplace_back(star);
        }
      }

      // print progress status
      if (mStars.size() % 10000 == 0) {
        spdlog::info("Read {} stars so far...", mStars.size());
      }
    }
    file.close();
    success = true;

    spdlog::info("Read a total of {} stars.", mStars.size());
  } else {
    spdlog::error("Failed to open catalog file {}!", sFilename);
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::writeStarCache(const std::string& sCacheFile) const {
  VistaType::uint32 catalogs = 0;
  for (std::map<CatalogType, std::string>::const_iterator it(mCatalogs.begin());
       it != mCatalogs.end(); ++it) {
    catalogs += (uint32_t)std::pow(2, (int)it->first);
  }

  VistaByteBufferSerializer serializer;
  serializer.WriteInt32((VistaType::uint32)cCacheVersion); // cache format version number
  serializer.WriteInt32((VistaType::uint32)catalogs);      // cache format version number
  serializer.WriteInt32(
      (VistaType::uint32)mStars.size()); // write number of stars to front of byte stream

  for (std::vector<Star>::const_iterator it = mStars.begin(); it != mStars.end(); ++it) {
    // serialize star data into byte stream
    serializer.WriteFloat32(it->mVMagnitude);
    serializer.WriteFloat32(it->mBMagnitude);
    serializer.WriteFloat32(it->mAscension);
    serializer.WriteFloat32(it->mDeclination);
    serializer.WriteFloat32(it->mParallax);
  }

  // open file
  std::ofstream file;
  file.open(sCacheFile.c_str(), std::ios::out | std::ios::binary);
  if (file.is_open()) {
    // write serialized star data
    spdlog::info("Writing {} stars ({} bytes) into {}", mStars.size(), serializer.GetBufferSize(),
        sCacheFile);
    file.write((const char*)serializer.GetBuffer(), serializer.GetBufferSize());
    file.close();
  } else {
    spdlog::error("Failed to open file {} for writing binary star data!", sCacheFile);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Stars::readStarCache(const std::string& sCacheFile) {
  bool success = false;

  // open file
  std::ifstream file;
  file.open(sCacheFile.c_str(),
      std::ios::in | std::ios::binary | std::ios::ate); // ate = set read pointer to end
  if (file.is_open()) {
    // read binary data from file
    int                          size = (int)file.tellg(); // get position of read pointer
    std::vector<VistaType::byte> data(size);
    file.seekg(0, std::ios::beg);       // set read pointer to the beginning of file stream
    file.read((char*)(&data[0]), size); // read file stream into char array 'data'
    file.close();

    // de-serialize byte stream
    VistaType::uint32 cacheVersion = 0;
    VistaType::uint32 catalogs     = 0;
    VistaType::uint32 numStars     = 0;

    VistaByteBufferDeSerializer deserializer;
    deserializer.SetBuffer(&data[0], size); // prepare for de-serialization
    deserializer.ReadInt32(cacheVersion);   // read cache format version number
    deserializer.ReadInt32(catalogs);       // read which catalogs were loaded
    deserializer.ReadInt32(numStars);       // read number of stars from front of byte stream

    if (cacheVersion != cCacheVersion) {
      return false;
    }

    VistaType::uint32 catalogsToLoad = 0;
    for (std::map<CatalogType, std::string>::const_iterator it(mCatalogs.begin());
         it != mCatalogs.end(); ++it) {
      catalogsToLoad += (uint32_t)std::pow(2, (int)it->first);
    }

    if (catalogs != catalogsToLoad) {
      return false;
    }

    for (unsigned int num = 0; num < numStars; ++num) {
      Star star;
      deserializer.ReadFloat32(star.mVMagnitude);
      deserializer.ReadFloat32(star.mBMagnitude);
      deserializer.ReadFloat32(star.mAscension);
      deserializer.ReadFloat32(star.mDeclination);
      deserializer.ReadFloat32(star.mParallax);

      mStars.emplace_back(star);

      // print progress status
      if (mStars.size() % 100000 == 0) {
        spdlog::info("Read {} stars so far...", mStars.size());
      }
    }

    success = true;

    spdlog::info("Read a total of {} stars.", mStars.size());
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::buildStarVAO() {
  int                c(0);
  const int          iElementCount(7);
  std::vector<float> data(iElementCount * mStars.size());

  for (auto it = mStars.begin(); it != mStars.end(); ++it, c += iElementCount) {
    // use B and V magnitude to retrieve the according color
    const float minIdx(-0.4f);
    const float maxIdx(2.0f);
    const float step(0.05f);
    float       bvIndex = std::min(maxIdx, std::max(minIdx, it->mBMagnitude - it->mVMagnitude));
    float       normalizedIndex = (bvIndex - minIdx) / (maxIdx - minIdx) / step + 0.5f;
    VistaColor  color           = mSpectralColors[(int)normalizedIndex];

    // distance in parsec --- some have parallax of zero; assume a
    // large distance in those cases
    float fDist = 100000.f;

    if (it->mParallax > 0.f) {
      fDist = 1000.f / it->mParallax;
    }

    data[c]     = it->mDeclination;
    data[c + 1] = it->mAscension;
    data[c + 2] = fDist;
    data[c + 3] = color.GetRed();
    data[c + 4] = color.GetGreen();
    data[c + 5] = color.GetBlue();
    data[c + 6] = it->mVMagnitude;

    mLoadedMinMagnitude = std::min(mLoadedMinMagnitude, it->mVMagnitude);
    mLoadedMaxMagnitude = std::max(mLoadedMaxMagnitude, it->mVMagnitude);
  }

  mStarVBO->Bind(GL_ARRAY_BUFFER);
  mStarVBO->BufferData(iElementCount * mStars.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
  mStarVBO->Release();

  // star positions
  mStarVAO->EnableAttributeArray(0);
  mStarVAO->SpecifyAttributeArrayFloat(
      0, 2, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 0, mStarVBO);

  // star distances
  mStarVAO->EnableAttributeArray(1);
  mStarVAO->SpecifyAttributeArrayFloat(
      1, 1, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 2 * sizeof(float), mStarVBO);

  // color
  mStarVAO->EnableAttributeArray(2);
  mStarVAO->SpecifyAttributeArrayFloat(
      2, 3, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 3 * sizeof(float), mStarVBO);

  // magnitude
  mStarVAO->EnableAttributeArray(3);
  mStarVAO->SpecifyAttributeArrayFloat(
      3, 1, GL_FLOAT, GL_FALSE, iElementCount * sizeof(float), 6 * sizeof(float), mStarVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Stars::buildBackgroundVAO() {
  std::vector<float> data(8);
  data[0] = -1;
  data[1] = 1;
  data[2] = 1;
  data[3] = 1;
  data[4] = -1;
  data[5] = -1;
  data[6] = 1;
  data[7] = -1;

  mBackgroundVBO->Bind(GL_ARRAY_BUFFER);
  mBackgroundVBO->BufferData(data.size() * sizeof(float), &(data[0]), GL_STATIC_DRAW);
  mBackgroundVBO->Release();

  // positions
  mBackgroundVAO->EnableAttributeArray(0);
  mBackgroundVAO->SpecifyAttributeArrayFloat(
      0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0, mBackgroundVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::stars
