/* global IApi, CosmoScout */

/**
 * Stars Api
 */
class StarsApi extends IApi {
  /**
   * @inheritDoc
   */
  name = 'stars';

  /**
   * @inheritDoc
   */
  init() {
    CosmoScout.initSlider("stars.setMagnitude", -10.0, 20.0, 0.1, [ -5, 15 ]);
    CosmoScout.initSlider("stars.setSize", 0.01, 1, 0.01, [ 0.05 ]);
    CosmoScout.initSlider("stars.setLuminanceBoost", 0.0, 20.0, 0.1, [ 0 ]);
  }
}

(() => {
  CosmoScout.init(StarsApi);
})();