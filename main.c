#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "raylib.h"

#define TRAIL_SIZE 300
#define NUM_ORBITERS 300

typedef struct {
  float azimuth;
  float elevation;
  float speedAz;
  float speedEl;
  float accelAz;
  float accelEl;
  float radius;
  Vector3 trail[TRAIL_SIZE];
  int trailIndex;
} Orbiter;

typedef struct {
  Vector3 position;
  Orbiter orbiters[NUM_ORBITERS];
  Color startColor;
  Color endColor;
  float coreRadius;
  float orbitInnerRadius;
  float orbitMiddleRadius;
  float orbitOuterRadius;
} RasenganEffect;

static float r2(void) { return (float)drand48(); }

static Color ColorLerp(Color c1, Color c2, float t) {
  return (Color){(unsigned char)(c1.r + (c2.r - c1.r) * t),
                 (unsigned char)(c1.g + (c2.g - c1.g) * t),
                 (unsigned char)(c1.b + (c2.b - c1.b) * t),
                 (unsigned char)(c1.a + (c2.a - c1.a) * t)};
}

RasenganEffect InitRasengan(Vector3 position, Color startColor, Color endColor,
                            float coreRadius, float innerRadius,
                            float middleRadius, float outerRadius) {
  RasenganEffect effect = {0};
  effect.position = position;
  effect.startColor = startColor;
  effect.endColor = endColor;
  effect.coreRadius = coreRadius;
  effect.orbitInnerRadius = innerRadius;
  effect.orbitMiddleRadius = middleRadius;
  effect.orbitOuterRadius = outerRadius;

  for (int i = 0; i < NUM_ORBITERS; i++) {
    effect.orbiters[i].azimuth = r2() * 2.0f * PI;
    effect.orbiters[i].elevation = r2() * PI;
    effect.orbiters[i].speedAz = 0.01f + r2() * 0.02f;
    effect.orbiters[i].speedEl = -0.015f + r2() * 0.03f;
    effect.orbiters[i].accelAz = 0.0f;
    effect.orbiters[i].accelEl = 0.0f;

    if (i < NUM_ORBITERS * 0.4f) {
      effect.orbiters[i].radius = innerRadius;
    } else if (i < NUM_ORBITERS * 0.6f) {
      effect.orbiters[i].radius = middleRadius;
    } else {
      effect.orbiters[i].radius = outerRadius;
    }

    effect.orbiters[i].trailIndex = 0;
    for (int j = 0; j < TRAIL_SIZE; j++) {
      effect.orbiters[i].trail[j] = position;
    }
  }

  return effect;
}

void UpdateRasenganOrbiters(RasenganEffect *effect) {
  for (int i = 0; i < NUM_ORBITERS; i++) {
    Orbiter *orb = &effect->orbiters[i];

    orb->accelAz = -0.0005f + r2() * 0.001f;
    orb->accelEl = -0.0005f + r2() * 0.001f;
    orb->speedAz += orb->accelAz;
    orb->speedEl += orb->accelEl;

    orb->speedAz = fmaxf(-0.05f, fminf(0.05f, orb->speedAz));
    orb->speedEl = fmaxf(-0.05f, fminf(0.05f, orb->speedEl));

    orb->azimuth += orb->speedAz;
    orb->elevation += orb->speedEl;

    orb->azimuth = fmodf(orb->azimuth + 2.0f * PI, 2.0f * PI);

    if (orb->elevation < 0.0f) {
      orb->elevation = -orb->elevation;
      orb->speedEl = -orb->speedEl;
    } else if (orb->elevation > PI) {
      orb->elevation = 2.0f * PI - orb->elevation;
      orb->speedEl = -orb->speedEl;
    }

    float r = orb->radius;
    float theta = orb->azimuth;
    float phi = orb->elevation;

    Vector3 relativePos = {r * sinf(phi) * cosf(theta), r * cosf(phi),
                           r * sinf(phi) * sinf(theta)};

    Vector3 pos = {relativePos.x + effect->position.x,
                   relativePos.y + effect->position.y,
                   relativePos.z + effect->position.z};

    orb->trail[orb->trailIndex] = pos;
    orb->trailIndex = (orb->trailIndex + 1) % TRAIL_SIZE;
  }
}

void DrawRasengan(const RasenganEffect *effect) {
  DrawSphere(effect->position, effect->coreRadius, (Color){255, 255, 255, 180});

  for (int i = 0; i < NUM_ORBITERS; i++) {
    const Orbiter *orb = &effect->orbiters[i];

    int count =
        (orb->trail[TRAIL_SIZE - 1].x == 0 &&
         orb->trail[TRAIL_SIZE - 1].y == 0 && orb->trail[TRAIL_SIZE - 1].z == 0)
            ? orb->trailIndex
            : TRAIL_SIZE;

    if (count > 1) {
      int startIndex = orb->trailIndex;
      Vector3 prevPoint = orb->trail[startIndex];

      for (int j = 1; j < count; j++) {
        int currIndex = (startIndex + j) % TRAIL_SIZE;
        float t = (float)j / (count - 1);
        Color segmentColor = ColorLerp(effect->startColor, effect->endColor, t);
        if (prevPoint.x == effect->position.x &&
            prevPoint.y == effect->position.y &&
            prevPoint.z == effect->position.z) {
          prevPoint = orb->trail[currIndex];
        }

        DrawLine3D(prevPoint, orb->trail[currIndex], segmentColor);
        prevPoint = orb->trail[currIndex];
      }
    }
  }
}

int main(void) {
  srand48(time(NULL));
  InitWindow(800, 600, "Multiple Rasengan Effects");

  Camera3D camera = {0};
  camera.position = (Vector3){4.0f, 3.0f, 4.0f};
  camera.target = (Vector3){0.0f, 1.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  RasenganEffect rasengan1 =
      InitRasengan((Vector3){0.0f, 1.0f, 0.0f},  // Position
                   (Color){0, 180, 255, 255},    // Start color (blue)
                   (Color){200, 255, 255, 255},  // End color (white)
                   0.3f,                         // Core radius
                   0.3f, 0.9f, 1.5f              // Layer radii
      );

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    UpdateRasenganOrbiters(&rasengan1);

    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode3D(camera);

    DrawRasengan(&rasengan1);

    DrawGrid(10, 1.0f);

    EndMode3D();

    DrawFPS(10, 40);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}