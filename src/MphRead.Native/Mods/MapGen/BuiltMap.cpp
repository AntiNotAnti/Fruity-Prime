#include "BuiltMap.hpp"

#include "MapDefinition.hpp"
#include "../../Formats/EntityClass.hpp"
#include "../../Formats/Types.hpp"

namespace MphRead::Mods::MapGen
{
    BuiltMap::BuiltMap(MapDefinition* definition) noexcept
        : _definitionOwner(definition == nullptr
              ? std::shared_ptr<MapDefinition>{}
              : definition->weak_from_this().lock()),
          _definition(definition)
    {
    }

    BuiltMap::~BuiltMap() = default;

    MapDefinition* BuiltMap::Definition() const noexcept
    {
        return _definition;
    }

    std::vector<BuiltFace*>& BuiltMap::Faces() noexcept
    {
        return _faces;
    }

    std::vector<BuiltFace*>& BuiltMap::Solid() noexcept
    {
        return _solid;
    }

    std::vector<Editor::EntityEditorBase*>& BuiltMap::Entities() noexcept
    {
        return _entities;
    }

    BuiltFace* BuiltMap::OwnFace(std::unique_ptr<BuiltFace> face)
    {
        BuiltFace* result = face.get();
        _ownedFaces.push_back(std::move(face));
        return result;
    }

    Editor::EntityEditorBase* BuiltMap::OwnEntity(
        std::unique_ptr<Editor::EntityEditorBase> entity)
    {
        Editor::EntityEditorBase* result = entity.get();
        _ownedEntities.push_back(std::move(entity));
        return result;
    }

    BuiltFace::BuiltFace(
        std::unique_ptr<MphRead::ManagedArray<OpenTK::Mathematics::Vector3>> points,
        std::unique_ptr<MphRead::ManagedArray<OpenTK::Mathematics::Vector2>> texcoords,
        OpenTK::Mathematics::Vector3 normal,
        std::int32_t material,
        float shade) noexcept
        : _points(std::move(points)),
          _texcoords(std::move(texcoords)),
          _normalX(normal.X),
          _normalY(normal.Y),
          _normalZ(normal.Z),
          _material(material),
          _shade(shade)
    {
    }

    BuiltFace::~BuiltFace() = default;

    MphRead::ManagedArray<OpenTK::Mathematics::Vector3>* BuiltFace::Points() const noexcept
    {
        return _points.get();
    }

    MphRead::ManagedArray<OpenTK::Mathematics::Vector2>* BuiltFace::Texcoords() const noexcept
    {
        return _texcoords.get();
    }

    OpenTK::Mathematics::Vector3 BuiltFace::Normal() const noexcept
    {
        return {_normalX, _normalY, _normalZ};
    }

    std::int32_t BuiltFace::Material() const noexcept
    {
        return _material;
    }

    float BuiltFace::Shade() const noexcept
    {
        return _shade;
    }

    bool BuiltFace::Damaging() const noexcept
    {
        return _damaging;
    }

    void BuiltFace::Damaging(bool value) noexcept
    {
        _damaging = value;
    }

    MphRead::Terrain BuiltFace::Terrain() const noexcept
    {
        return _terrain;
    }

    void BuiltFace::Terrain(MphRead::Terrain value) noexcept
    {
        _terrain = value;
    }
}
