/* Copyright 2015 Jonathan Dahan <jonathan@jonathan.is>
 * Copyright (c) 2012 Dan Wilcox <danomatika@gmail.com>
 *
 * BSD Simplified License. See LICENSE.txt for details.
 */
#include "ofxSceneManager.h"

ofxSceneManager::ofxSceneManager() :
  _currentScene(SCENE_NONE), _newScene(SCENE_NOCHANGE),
  _bChangeNow(false), _minChangeTimeMS(100), _bSignalledAutoChange(false)
{
  _sceneChangeTimer.set();
  _currentScenePtr = NULL;
}

ofxScene* ofxSceneManager::add(ofxScene* scene) {
  if(scene == NULL) {
    ofLogWarning("ofxSceneManager") << "cannot add NULL scene";
    return NULL;
  }

  if(_scenes.find(scene->getName()) != _scenes.end()) {
    ofLogWarning("ofxSceneManager") << "scene \"" << scene->getName()
                   << "\" already added, only unique names allowed";
    return NULL;
  }

  _scenes.insert(_scenes.end(), pair<std::string,ofxScene::RunnerScene*>(scene->getName(), new ofxScene::RunnerScene(scene)));
  return scene;
}

void ofxSceneManager::remove(ofxScene* scene) {
  if(scene == NULL) {
    ofLogWarning("ofxSceneManager") << "cannot remove NULL scene";
    return;
  }

  map<std::string,ofxScene::RunnerScene*>::iterator iter;
  map<std::string,ofxScene::RunnerScene*>::iterator diter;
  for(iter = _scenes.begin(); iter != _scenes.end(); ++iter) {
    ofxScene::RunnerScene* s = (*iter).second;
    if(s->scene == scene) {
      if(s != NULL) {
        s->exit();
        delete s;
      }
      diter = iter;
      _scenes.erase(diter);
      return;
    }
  }

}

void ofxSceneManager::clear() {
  map<std::string,ofxScene::RunnerScene*>::iterator iter;
  for(iter = _scenes.begin(); iter != _scenes.end(); ++iter) {
    ofxScene::RunnerScene* s = (*iter).second;
    if(s != NULL) {
      s->exit();
      delete s;
    }
  }
  _scenes.clear();
}

// need to call ofxScene::RunnerScene::setup()
void ofxSceneManager::setup(bool loadAll) {
  if(loadAll) {
    map<std::string,ofxScene::RunnerScene*>::iterator iter;
    for(iter = _scenes.begin(); iter != _scenes.end(); ++iter) {
      ofxScene::RunnerScene* s = (*iter).second;
      s->setup();
    }
  } else {  // load the current one only
    if(!_scenes.empty() && _currentScene >= 0) {
      _currentRunnerScenePtr->setup();
    }
  }
}

void ofxSceneManager::run(bool run) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->run(run);
    ofLogVerbose("ofxSceneManager") << "SCENE " << _currentScenePtr->getName()
            << " RUN: " << _currentScenePtr->isRunning();
  }
}

void ofxSceneManager::runToggle() {
  run(!isRunning());
}

bool ofxSceneManager::isRunning() {
  if(!_scenes.empty() && _currentScene >= 0) {
    return _currentScenePtr->isRunning();
  }
  return false;
}

void ofxSceneManager::noScene(bool now) {
  if(_sceneChangeTimer.getDiff() < _minChangeTimeMS)
    return;

  if(!now && _currentScene > -1) {
    _currentScenePtr->startExiting();
  }
  _bChangeNow = now;
  _newScene = SCENE_NONE;
  ofLogVerbose("ofxSceneManager") << "NO SCENE";
}

void ofxSceneManager::nextScene(bool now) {
  if(_currentScene+1 >= (int) _scenes.size()) {
    gotoScene(0, now);
  } else {
    gotoScene(_currentScene+1, now);
  }
}

void ofxSceneManager::prevScene(bool now) {
  if(_currentScene-1 < 0) {
    gotoScene(_scenes.size()-1, now);
  } else {
    gotoScene(_currentScene-1, now);
  }
}

void ofxSceneManager::gotoScene(unsigned int index, bool now) {
  if(_scenes.empty() || index >= _scenes.size() ||
     _sceneChangeTimer.getDiff() < _minChangeTimeMS)
    return;

  ofxScene* s;
  if(_currentScene == (int) index) {
    ofLogWarning("ofxSceneManager") << "ignoring duplicate goto scene change";
    return;
  }

  if(!now) {

    // tell current scene to exit
    if(_currentScene > SCENE_NONE) {
      s = _currentScenePtr;
      s->startExiting();
    }

    // tell new scene to enter
    s = getSceneAt(index);
    s->startEntering();
  }

  _newScene = index;
  _bChangeNow = now;
  ofLogVerbose("ofxSceneManager") << "GOTO SCENE: " << _newScene;
}

// using the std::distance func to turn iter into index
void ofxSceneManager::gotoScene(std::string name, bool now) {
  map<std::string,ofxScene::RunnerScene*>::iterator iter = _scenes.find(name);
  if(iter == _scenes.end()) {
    ofLogWarning("ofxSceneManager") << "could not find \"" << name << "\"";
    return;
  }
  gotoScene(std::distance(_scenes.begin(), iter), now);
}

ofxScene* ofxSceneManager::getScene(std::string name) {
  map<std::string,ofxScene::RunnerScene*>::iterator iter = _scenes.find(name);
  return iter != _scenes.end() ? iter->second->scene : NULL;
}

ofxScene* ofxSceneManager::getSceneAt(unsigned int index) {
  ofxScene::RunnerScene* rs = _getRunnerSceneAt(index);
  return rs == NULL ? NULL : rs->scene;
}

std::string ofxSceneManager::getSceneName(unsigned int index) {
  ofxScene* s = getSceneAt(index);
  return s == NULL ? "" : s->getName();
}

// using the std::distance func to turn iter into index
int ofxSceneManager::getSceneIndex(std::string name) {
  map<std::string,ofxScene::RunnerScene*>::iterator iter = _scenes.find(name);
  return iter != _scenes.end() ? std::distance(_scenes.begin(), iter) : -1;
}

ofxScene* ofxSceneManager::getCurrentScene() {
  return _currentScenePtr;
}

std::string ofxSceneManager::getCurrentSceneName() {
  return _currentScenePtr == NULL ? "NO_SCENE" : _currentScenePtr->getName();
}

int ofxSceneManager::getCurrentSceneIndex() {
  return _currentScene;
}

unsigned int ofxSceneManager::getMinChangeTime() {
  return _minChangeTimeMS;
}

void ofxSceneManager::setMinChangeTime(unsigned int time) {
  _minChangeTimeMS = time;
}

// need to call ofxScene::RunnerScene::update()
void ofxSceneManager::update() {

  _handleSceneChanges();

  // update the current main scene
  if(!_scenes.empty() && _currentScene >= 0) {

    ofxScene* s = _currentScenePtr;

    // call setup if scene is not setup yet
    if(!s->isSetup()) {
      _currentRunnerScenePtr->setup();
    }

    _currentRunnerScenePtr->update();

    // if this scene says it's done, go to the next one
    if(s->isDone() && !_bSignalledAutoChange) {
      nextScene();
      _bSignalledAutoChange = true;
    }
  }
}

// need to call ofxScene::RunnerScene::draw()
void ofxSceneManager::draw() {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentRunnerScenePtr->draw();
  }
}

void ofxSceneManager::keyPressed(int key) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->keyPressed(key);
  }
}

void ofxSceneManager::keyReleased(int key) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->keyReleased(key);
  }
}

void ofxSceneManager::mouseMoved(int x, int y ) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->mouseMoved(x, y);
  }
}

void ofxSceneManager::mouseDragged(int x, int y, int button) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->mouseDragged(x, y, button);
  }
}

void ofxSceneManager::mousePressed(int x, int y, int button) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->mousePressed(x, y, button);
  }
}

void ofxSceneManager::mouseReleased(int x, int y, int button) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->mouseReleased(x, y, button);
  }
}

// call resize on all scenes
void ofxSceneManager::windowResized(int w, int h) {
  map<std::string,ofxScene::RunnerScene*>::iterator iter;
  for(iter = _scenes.begin(); iter != _scenes.end(); ++iter) {
    ofxScene::RunnerScene* s = (*iter).second;
    s->windowResized(w, h);
  }
}

void ofxSceneManager::dragEvent(ofDragInfo dragInfo) {
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->dragEvent(dragInfo);
  }
}

void ofxSceneManager::gotMessage(ofMessage msg){
  if(!_scenes.empty() && _currentScene >= 0) {
    _currentScenePtr->gotMessage(msg);
  }
}

void ofxSceneManager::_handleSceneChanges() {

  // do the actual main scene change
  if(_newScene != SCENE_NOCHANGE) {

    // ignore duplicates
    if(_newScene == _currentScene) {

      if(_newScene == SCENE_NONE) { // nothing to do
        return;
      }

      // not done entering
      if(_currentScenePtr->isEntering()) {
        _newScene = SCENE_NOCHANGE;
        ofLogWarning("ofxSceneManager") << "ignoring duplicate scene change, "
              << "current scene is not done entering";
        _sceneChangeTimer.set();
      }
    }

    // only change to the new scene if the old scene is done exiting
    if(_currentScene > SCENE_NONE) {
      if(_bChangeNow || !_currentScenePtr->isExiting()) {
        _currentRunnerScenePtr->exit();
        changeToNewScene();
      }
    } else {   // no current scene to wait for
      changeToNewScene();
    }
  }
}

void ofxSceneManager::changeToNewScene() {

  _currentScene = _newScene;
  _currentRunnerScenePtr = _getRunnerSceneAt(_currentScene);

  if(_currentRunnerScenePtr) {
    _currentScenePtr = _currentRunnerScenePtr->scene;
    ofLogVerbose("ofxSceneManager") << "changed to " << _currentScene
      << " \"" << _currentScenePtr->getName() << "\"";
  }
  else {
    _currentScenePtr = NULL;
    ofLogVerbose("ofxSceneManager") << "changed to NO_SCENE";
  }

  _newScene = SCENE_NOCHANGE; // done
  _bSignalledAutoChange = false;
  _sceneChangeTimer.set();
}

ofxScene::RunnerScene* ofxSceneManager::_getRunnerSceneAt(int index) {
  if(index < _scenes.size()) {
    map<std::string,ofxScene::RunnerScene*>::iterator iter = _scenes.begin();
    std::advance(iter, index);
    return (*iter).second;
  }
  return NULL;
}
