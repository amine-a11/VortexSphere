#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "raylib.h"
#include "raymath.h"

#define TRAIL_SIZE 300
#define NUM_ORBITERS 300

#define READ_END 0
#define WRITE_END 1

#define WIDTH 800
#define HEIGHT 600
uint32_t pixels[WIDTH * HEIGHT];

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

void UpdateCam(Camera3D *camera) {
  const float sensitivityOrbit = 0.005f;
  const float sensitivityZoom = 1.0f;
  const float sensitivityPan = 0.005f;

  Vector3 offset = Vector3Subtract(camera->position, camera->target);
  float radius = Vector3Length(offset);

  float azimuth = atan2f(offset.z, offset.x);  // horizontal angle
  float elevation = acosf(offset.y / radius);  // vertical angle

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    Vector2 delta = GetMouseDelta();
    azimuth -= delta.x * sensitivityOrbit;
    elevation -= delta.y * sensitivityOrbit;
    if (elevation < 0.1f) elevation = 0.1f;
    if (elevation > PI - 0.1f) elevation = PI - 0.1f;
  }

  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    radius -= wheel * sensitivityZoom;
    if (radius < 0.5f) radius = 0.5f;
    if (radius > 100.f) radius = 100.f;
  }

  offset.x = radius * sinf(elevation) * cosf(azimuth);
  offset.y = radius * cosf(elevation);
  offset.z = radius * sinf(elevation) * sinf(azimuth);

  camera->position = Vector3Add(camera->target, offset);

  if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
    Vector2 deltaPan = GetMouseDelta();

    Vector3 forward =
        Vector3Normalize(Vector3Subtract(camera->target, camera->position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

    Vector3 pan = {-deltaPan.x * sensitivityPan, deltaPan.y * sensitivityPan,
                   0.0f};
    Vector3 panWorld =
        Vector3Add(Vector3Scale(right, pan.x), Vector3Scale(up, pan.y));

    camera->target = Vector3Add(camera->target, panWorld);
    camera->position = Vector3Add(camera->position, panWorld);
  }
}

int main(void) {
  srand48(time(NULL));
  InitWindow(WIDTH, HEIGHT, "Multiple Rasengan Effects");

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

    UpdatePeasyCam(&camera);

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
int main2(void) {
  int pipefd[2];
  if (pipe(pipefd) < -1) {
    return 1;
    printf("pipe failed\n");
  }

  pid_t child = fork();
  if (child < 0) {
    printf("fork failed\n");
    return 1;
  }

  if (child == 0) {
    if (dup2(pipefd[READ_END], STDIN_FILENO) < 0) {
      printf("dup2 failed\n");
      return 1;
    }
    close(pipefd[WRITE_END]);
    int ret =
        execlp("ffmpeg", "ffmpeg", "-loglevel", "verbose", "-y", "-f",
               "rawvideo", "-pix_fmt", "rgba", "-s", "800x600", "-r", "60",
               "-an", "-i", "-", "-c:v", "libx264", "output.mp4", NULL);

    if (ret < 0) {
      printf("exec failed\n");
      return 1;
    }
  }
  close(pipefd[READ_END]);
  srand48(time(NULL));
  InitWindow(WIDTH, HEIGHT, "Multiple Rasengan Effects");

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
  RenderTexture2D screen = LoadRenderTexture(WIDTH, HEIGHT);

  // while (!WindowShouldClose()) {
  for (size_t i = 0; i < 60 * 30 && !WindowShouldClose(); i++) {
    UpdateRasenganOrbiters(&rasengan1);
    UpdateCamera(&camera, CAMERA_ORBITAL);

    BeginDrawing();
    BeginTextureMode(screen);
    ClearBackground(BLACK);

    BeginMode3D(camera);

    DrawRasengan(&rasengan1);

    DrawGrid(10, 1.0f);

    EndMode3D();

    // DrawFPS(10, 40);
    EndTextureMode();
    DrawTexture(screen.texture, 0, 0, WHITE);
    EndDrawing();

    Image image = LoadImageFromTexture(screen.texture);
    ImageFlipVertical(&image);  // Flip the image vertically
    write(pipefd[WRITE_END], image.data, WIDTH * HEIGHT * sizeof(uint32_t));
    UnloadImage(image);
  }

  CloseWindow();
  close(pipefd[WRITE_END]);
  wait(NULL);
  printf("Done rendering the image\n");

  return 0;
}