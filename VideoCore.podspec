Pod::Spec.new do |s|
  s.name                = "VideoCore"
  s.version             = "0.1.2"
  s.summary             = "An audio and video manipulation pipeline for iOS and Mac OS X."
  s.description      = <<-DESC
                          This is a work-in-progress library with the
                          intention of being an audio and video manipulation
                          pipeline for iOS and Mac OS X.
                          DESC
  s.homepage            = "https://github.com/jamesghurley/VideoCore"
  s.license             = 'LGPL 2.1'
  s.authors             = { "James Hurley" => "jamesghurley@gmail.com" }
  s.source              = { :git => "https://github.com/jamesghurley/VideoCore.git", :tag => s.version.to_s }

  s.requires_arc        = false

  s.header_dir          = 'videocore'
  s.header_mappings_dir = '.'

  s.source_files        = [ 'mixers/**/*.h*', 'mixers/**/*.cpp', 'mixers/**/*.m*',
                            'rtmp/**/*.h*', 'rtmp/**/*.cpp', 'rtmp/**/*.m*',
                            'sources/**/*.h*', 'sources/**/*.cpp', 'sources/**/*.m*',
                            'stream/**/*.h*', 'stream/**/*.cpp', 'stream/**/*.m*',
                            'system/**/*.h*', 'system/**/*.cpp', 'system/**/*.m*',
                            'transforms/**/*.h*', 'transforms/**/*.cpp', 'transforms/**/*.m*' ]

  s.frameworks          = [ 'AudioToolbox', 'AVFoundation', 'CFNetwork', 'CoreMedia',
                            'CoreVideo', 'OpenGLES', 'Foundation', 'CoreGraphics' ]

  s.dependency          'boost', '~> 1.51.0'
  s.dependency          'glm', '~> 0.9.4.6'
  s.dependency          'UriParser-cpp', '~> 0.1.3'

  s.xcconfig            = { "HEADER_SEARCH_PATHS" => "${PODS_ROOT}/boost" }

  s.ios.deployment_target = '5.0'

  # Before we can get OS X deployment working, we'll need to use sub-specs to
  # separate out the source files for OS X vs. iOS
  #s.osx.deployment_target = '10.7'
end
