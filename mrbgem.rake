MRuby::Gem::Specification.new('mruby-esp32-spi-ssd1306') do |spec|
  spec.license = 'MIT'
  spec.authors = 'icm7216'

  spec.cc.include_paths << "#{build.root}/src"
end
