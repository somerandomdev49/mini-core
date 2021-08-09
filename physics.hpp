#ifndef PHYSICS
#define PHYSICS
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <RigidBox/RigidBox.h>
#include <reactphysics3d/reactphysics3d.h>

/** Namespace for communicating with physics libraries...
 * Libraries:
 *  * RigidBox
 *  * ReactPhysics3D
 * more to come...? D:
*/
namespace physics
{
    namespace rp3d = reactphysics3d;

    /** Converts a glm vector to an RP3D vector */
    rp3d::Vector3 glm2rp3d(const glm::vec3 &v)
    { return rp3d::Vector3(v.x, v.y, v.z); }

    /** Converts a glm quaternion to an RP3D quaternion */
    rp3d::Quaternion glm2rp3d(const glm::quat &v)
    { return rp3d::Quaternion(v.x, v.y, v.z, v.w); } /** NOTE: Order? */

    



    /** Converts a Rigidbox vector to a glm vector */
    glm::vec3 rb2glm(const rbVec3 &v)
    { return { v.x, v.y, v.z }; }

    /** Converts a Rigidbox matrix to a glm quaternion */
    glm::quat rb2glm(const rbMtx3 &v)
    {
        return glm::quat_cast(glm::mat3 { rb2glm(v.r[0]), rb2glm(v.r[1]), rb2glm(v.r[2]) });
    }

    /** Converts a glm vector to a Rigidbox vector */
    rbVec3 glm2rb(const glm::vec3 &v)
    { return { v.x, v.y, v.z }; }

    /** Converts a glm matrix to a Rigidbox matrix */
    rbMtx3 glm2rb(const glm::mat3 &v)
    {
        rbMtx3 m;
        m.r[0] = glm2rb(v[0]);
        m.r[1] = glm2rb(v[1]);
        m.r[2] = glm2rb(v[2]);
        return m;
    }
}

#endif // PHYSICS
