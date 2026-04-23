#ifndef FE_MODEL3D_HPP
#define FE_MODEL3D_HPP

#include <memory>
#include <string>
#include <vector>

#include "fe_image.hpp"

class FeModel3D;
class FeShader;

class FeModel3DMaterialArtwork
{
public:
	FeModel3DMaterialArtwork( FeModel3D &model, const std::string &material_name );

	const char *get_material_name() const;
	int get_index_offset() const;
	void set_index_offset( int io );
	int get_filter_offset() const;
	void set_filter_offset( int fo );
	void rawset_index_offset( int io );
	void rawset_filter_offset( int fo );
	int get_video_flags() const;
	void set_video_flags( int flags );
	bool get_movie_enabled() const;
	void set_movie_enabled( bool enabled );
	bool get_video_playing() const;
	void set_video_playing( bool playing );
	int get_video_duration() const;
	int get_video_time() const;
	bool get_preserve_aspect_ratio() const;
	void set_preserve_aspect_ratio( bool preserve );
	const char *get_file_name() const;
	void set_file_name( const char *filename );
	int get_trigger() const;
	void set_trigger( int trigger );
	float get_volume() const;
	void set_volume( float volume );
	float get_pan() const;
	void set_pan( float pan );
	bool get_smooth() const;
	void set_smooth( bool smooth );
	bool get_mipmap() const;
	void set_mipmap( bool mipmap );
	bool get_repeat() const;
	void set_repeat( bool repeat );
	float get_sample_aspect_ratio() const;
	FeShader *get_shader() const;
	void set_shader( FeShader *shader );
	void set_artwork( const char *artwork_label );
	void set_file( const char *filename );
	void clear();

private:
	FeModel3D *m_model;
	std::string m_material_name;
};

class FeModel3D : public FeBasePresentable
{
public:
	FeModel3D( FePresentableParent &p, const std::string &filename );
	FeModel3D( FeModel3D *o, FePresentableParent &p );
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

	FeModel3DMaterialArtwork *get_material_artwork( const char *material_name );
	FeModel3DMaterialArtwork *add_material_artwork( const char *material_name, const char *artwork_label );
	FeModel3DMaterialArtwork *add_material_image( const char *material_name, const char *filename );
	void clear_material_texture( const char *material_name );

	bool build_render_geometry( std::vector<FeRenderGeometry> &geometry ) const;

private:
	struct ModelData;
	struct MaterialOverride;

	void load_model( const std::string &filename );
	void initialize_override_defaults( MaterialOverride &entry );
	void apply_override_settings( MaterialOverride &entry, bool do_update = true );
	FeTextureContainer *create_override_artwork_container( const char *artwork_label ) const;
	FeTextureContainer *create_override_file_container( const char *filename ) const;
	MaterialOverride *find_override( const std::string &material_name );
	const MaterialOverride *find_override( const std::string &material_name ) const;
	MaterialOverride *find_or_create_override( const std::string &material_name );
	void set_override_container( const std::string &material_name, FeTextureContainer *container );
	void clear_override( const std::string &material_name );
	void release_overrides();
	void invalidate_geometry_cache() const;
	bool geometry_cache_matches( float camera_light ) const;
	void update_geometry_cache_state( float camera_light ) const;
	void update_cached_material_state( FeRenderGeometry &entry, std::size_t primitive_index ) const;
	void refresh_geometry_cache() const;
	void rebuild_geometry_cache( float camera_light ) const;

	std::string m_file_name;
	Vec2f m_pos;
	Vec2f m_size;
	Vec3f m_anchor;
	float m_depth;
	float m_rotation;
	Color m_color;
	std::shared_ptr<ModelData> m_model;
	std::vector<std::unique_ptr<MaterialOverride>> m_overrides;
	mutable bool m_geometry_cache_valid;
	mutable const ModelData *m_geometry_cache_model;
	mutable Vec2f m_geometry_cache_pos;
	mutable Vec2f m_geometry_cache_size;
	mutable Vec3f m_geometry_cache_anchor;
	mutable float m_geometry_cache_depth;
	mutable float m_geometry_cache_rotation;
	mutable float m_geometry_cache_z;
	mutable float m_geometry_cache_rotation_x;
	mutable float m_geometry_cache_rotation_y;
	mutable int m_geometry_cache_rotation_order;
	mutable Color m_geometry_cache_color;
	mutable bool m_geometry_cache_zbuffer;
	mutable float m_geometry_cache_camera_light;
	mutable std::vector<std::size_t> m_geometry_cache_primitives;
	mutable std::vector<FeRenderGeometry> m_geometry_cache;

	friend class FeModel3DMaterialArtwork;
};

#endif
