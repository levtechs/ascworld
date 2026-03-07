#include "world/prefabs/building_mounted/pipe_run.h"
#include <cmath>

static const float kPipeRadius    = 0.12f;
static const float kBracketW      = 0.18f;  // bracket box footprint
static const float kBracketH      = 0.12f;
static const float kBracketD      = 0.08f;
static const float kBracketSpacing = 1.5f;

void placePipeRun(
    const Vec3& origin, int axis, float length,
    const DistrictPalette& pal,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects)
{
    Material metalMat = pal.metal;

    // Center of the pipe is at origin + half-length along the chosen axis.
    // createCylinder is vertical (Y-axis) by default; we rotate to lie along X or Z.

    Vec3 pipeCenter = origin;
    float halfLen   = length * 0.5f;

    // Rotation to lay the cylinder along the requested axis.
    // Default cylinder axis = Y.  Rotate 90° around Z to get X-axis pipe,
    // or 90° around X to get Z-axis pipe.
    Mat4 axisRot;
    if (axis == 0) {
        // Lay along X: rotate around Z by -90°
        pipeCenter.x += halfLen;
        axisRot = Mat4::rotateZ(-1.5707963f);
    } else {
        // Lay along Z (axis == 2): rotate around X by 90°
        pipeCenter.z += halfLen;
        axisRot = Mat4::rotateX(1.5707963f);
    }

    // Scale the cylinder: radius=0.3 → kPipeRadius, height=1.8 → length
    Mat4 pipeTransform = Mat4::translate(pipeCenter)
                       * axisRot
                       * Mat4::scale({ kPipeRadius / 0.3f,
                                       length / 1.8f,
                                       kPipeRadius / 0.3f });

    objects.push_back({ meshCache.cylinder(0.3f, 1.8f, 8), pipeTransform, metalMat });

    // ---- End caps: small spheres at both pipe termination points ----
    {
        Vec3 startCap = origin;
        Vec3 endCap   = origin;
        if (axis == 0) {
            endCap.x += length;
        } else {
            endCap.z += length;
        }
        // Sphere cap at start
        Mat4 capStart = Mat4::translate(startCap);
        objects.push_back({ meshCache.sphere(4, 8, kPipeRadius * 1.15f), capStart, metalMat });
        // Sphere cap at end
        Mat4 capEnd = Mat4::translate(endCap);
        objects.push_back({ meshCache.sphere(4, 8, kPipeRadius * 1.15f), capEnd, metalMat });
    }

    // ---- Brackets every kBracketSpacing units along the pipe ----
    // Bracket is a flat box centred on the pipe, sitting just behind it
    // (negative local depth offset — they're wall-mounted).
    int numBrackets = static_cast<int>(length / kBracketSpacing) + 1;
    for (int i = 0; i < numBrackets; ++i) {
        float t = (numBrackets > 1)
                ? (static_cast<float>(i) / (numBrackets - 1)) * length
                : halfLen;

        Vec3 bPos = origin;
        if (axis == 0) {
            bPos.x += t;
            // Bracket wider in Z (across the pipe), slim in X, with depth in Y
            Mat4 bt = Mat4::translate(bPos)
                    * Mat4::scale({ kBracketD, kBracketH, kBracketW });
            objects.push_back({ meshCache.cube(), bt, metalMat });
        } else {
            bPos.z += t;
            Mat4 bt = Mat4::translate(bPos)
                    * Mat4::scale({ kBracketW, kBracketH, kBracketD });
            objects.push_back({ meshCache.cube(), bt, metalMat });
        }
    }
}
