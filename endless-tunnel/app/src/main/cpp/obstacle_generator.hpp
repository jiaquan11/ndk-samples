// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_obstacle_generator_hpp
#define endlesstunnel_obstacle_generator_hpp

#include "engine.hpp"
#include "obstacle.hpp"

// Generates obstacles given a difficulty level.
class ObstacleGenerator {
 private:
  int mDifficulty;

 public:
  ObstacleGenerator() { mDifficulty = 0; }

  void SetDifficulty(int dif) { mDifficulty = dif; }

  // generate a new obstacle.
  void Generate(Obstacle *result);

 private:
  void GenEasy(Obstacle *result);
  void GenMedium(Obstacle *result);
  void GenIntermediate(Obstacle *result);
  void GenHard(Obstacle *result);

  void FillRow(Obstacle *result, int row);
  void FillCol(Obstacle *result, int col);
};

#endif
