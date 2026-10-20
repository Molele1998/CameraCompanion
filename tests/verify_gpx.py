"""Parse files written by the actual GPX writer on the host, using synthetic GPS."""
import sys
from pathlib import Path
import xml.etree.ElementTree as ET
ns={'g':'http://www.topografix.com/GPX/1/1'}
files=list(Path(sys.argv[1]).rglob('*.gpx'))
assert len(files)>=2
segments=0
for path in files:
    root=ET.parse(path).getroot()
    assert root.tag=='{http://www.topografix.com/GPX/1/1}gpx' and root.attrib['version']=='1.1'
    assert root.find('g:metadata/g:name',ns).text=='SIMULATED GPS'
    segments=max(segments,len(root.findall('g:trk/g:trkseg',ns)))
    for point in root.findall('g:trk/g:trkseg/g:trkpt',ns):
        assert -90<=float(point.attrib['lat'])<=90 and -180<=float(point.attrib['lon'])<180
        assert point.find('g:time',ns).text.endswith('Z')
        assert point.find('g:ele',ns) is not None
assert segments>=2
print(f'PASS: independent XML parser: {len(files)} real host GPX files, namespace/metadata/points/time/segments')
