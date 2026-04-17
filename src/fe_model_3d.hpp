#ifndef FE_MODEL3D_HPP
#define FE_MODEL3D_HPP

#include <memory>
#include <string>
#include <vector>

#include "fe_image.hpp"

class FeModel3D : public FeBasePresentable
{
public:
	FeModel3D( FePresentableParent &p, const std::string &filename );
	~FeModel3D() override;

	Vec2f getPosition() const override;
	void setPosition( const Vec2f &pos ) override;
	Vec2f getSize() const override;
	void setSize( const Vec2f &size ) override;
	float getRotation() const override;
	void setRotation( float rotation ) override;
	Color getColor() const override;
	void setColor( Color color ) override;

	void set_anchor( float x, float y );
	void set_anchor( float x, float y, float z );
	void set_anchor_x( float x );
	float get_anchor_x() const;
	void set_anchor_y( float y );
	float get_anchor_y() const;
	void set_anchor_z( float z );
	float get_anchor_z() const;

	float get_depth() const;
	void set_depth( float depth );
	const char *get_file_name() const;

	void set_material_artwork( const char *material_name, const char *artwork_label );
	void set_material_file( const char *material_name, const char *filename );
	void set_material_texture_rotation( const char *material_name, float degrees );
	void clear_material_texture( const char *material_name );

	bool build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const;

private:
	struct ModelData;
	struct MaterialOverride;

	void load_model( const std::string &filename );
	MaterialOverride *find_override( const std::string &material_name );
	const MaterialOverride *find_override( const std::string &material_name ) const;
	MaterialOverride *find_or_create_override( const std::string &material_name );
	void set_override_container( const std::string &material_name, FeTextureContainer *container );
	void clear_override( const std::string &material_name );
	void release_overrides();

	std::string m_file_name;
	Vec2f m_pos;
	Vec2f m_size;
	Vec3f m_anchor;
	float m_depth;
	float m_rotation;
	Color m_color;
	std::shared_ptr<ModelData> m_model;
	std::vector<std::unique_ptr<MaterialOverride>> m_overrides;
};

#endif
